#include "commands/jobs.hpp"

#include "core/db.hpp"
#include "core/jobs.hpp"

#include <dpp/dpp.h>

#include <cstdint>
#include <optional>
#include <string>

namespace broom::commands {

namespace {

std::string status_emoji(const std::string& status) {
    if (status == "queued") return "⏳";
    if (status == "running") return "▶️";
    if (status == "done") return "✅";
    if (status == "cancelled") return "🛑";
    if (status == "failed") return "❌";
    return "•";
}

void handle_list(Services& services, const dpp::slashcommand_t& event) {
    auto stmt = services.db.prepare(
        "SELECT id, kind, status, scanned, matched, actioned, started_by, created_at "
        "FROM jobs WHERE guild_id=?1 ORDER BY id DESC LIMIT 10");
    stmt.bind(1, static_cast<std::int64_t>(static_cast<std::uint64_t>(event.command.guild_id)));

    std::string body;
    while (stmt.step()) {
        std::int64_t id = stmt.column_int(0);
        std::string kind = stmt.column_text(1);
        std::string status = stmt.column_text(2);
        std::int64_t scanned = stmt.column_int(3);
        std::int64_t matched = stmt.column_int(4);
        std::int64_t actioned = stmt.column_int(5);
        auto started_by = static_cast<std::uint64_t>(stmt.column_int(6));
        std::int64_t created = stmt.column_int(7);

        body += "`#" + std::to_string(id) + "` " + kind + " — " + status_emoji(status) +
                " " + status + " · " + std::to_string(scanned) + "/" +
                std::to_string(matched) + "/" + std::to_string(actioned) + " · <@" +
                std::to_string(started_by) + "> · <t:" + std::to_string(created) + ":R>\n";
    }
    if (body.empty()) body = "No jobs yet.";

    dpp::embed embed;
    embed.set_title("Background jobs")
        .set_description(body)
        .set_footer(dpp::embed_footer().set_text("scanned / matched / actioned"))
        .set_color(0x5865F2);
    event.reply(dpp::message().add_embed(embed).set_flags(dpp::m_ephemeral));
}

void handle_cancel(Services& services, const dpp::slashcommand_t& event,
                   const dpp::command_data_option& sub) {
    std::int64_t id = 0;
    for (const auto& o : sub.options) {
        if (o.name == "id" && std::holds_alternative<std::int64_t>(o.value)) {
            id = std::get<std::int64_t>(o.value);
        }
    }

    // Confirm the job belongs to this server before touching it.
    auto stmt = services.db.prepare("SELECT guild_id FROM jobs WHERE id=?1");
    stmt.bind(1, id);
    bool in_guild =
        stmt.step() && static_cast<std::uint64_t>(stmt.column_int(0)) ==
                           static_cast<std::uint64_t>(event.command.guild_id);
    if (!in_guild) {
        event.reply(dpp::message("No job `#" + std::to_string(id) + "` in this server.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    bool ok = services.jobs.request_cancel(id);
    event.reply(dpp::message(ok ? "🛑 Cancelling job `#" + std::to_string(id) + "`…"
                                : "Job `#" + std::to_string(id) +
                                      "` is not active (already finished?).")
                    .set_flags(dpp::m_ephemeral));
}

} // namespace

dpp::slashcommand Jobs::definition(dpp::snowflake app_id) const {
    dpp::slashcommand sc(name(), "View and manage background jobs", app_id);
    sc.set_default_permissions(dpp::p_manage_messages);
    sc.add_option(dpp::command_option(dpp::co_sub_command, "list", "List recent jobs"));

    dpp::command_option cancel(dpp::co_sub_command, "cancel", "Cancel a queued or running job");
    cancel.add_option(
        dpp::command_option(dpp::co_integer, "id", "Job ID (from /jobs list)", true));
    sc.add_option(cancel);
    return sc;
}

void Jobs::handle(const dpp::slashcommand_t& event) const {
    if (!event.command.guild_id) {
        event.reply(dpp::message("This command can only be used in a server.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    const auto& interaction = event.command.get_command_interaction();
    if (interaction.options.empty()) return;
    const auto& sub = interaction.options[0];

    if (sub.name == "list") {
        handle_list(*services_, event);
    } else if (sub.name == "cancel") {
        handle_cancel(*services_, event, sub);
    }
}

} // namespace broom::commands
