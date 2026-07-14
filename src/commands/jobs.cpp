#include "commands/jobs.hpp"

#include "commands/embeds.hpp"
#include "commands/options.hpp"
#include "commands/pagination.hpp"
#include "core/db.hpp"
#include "core/jobs.hpp"

#include <dpp/dpp.h>

#include <cstdint>
#include <string>
#include <vector>

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

// Shared by /jobs list and its pagination buttons.
dpp::message list_message(Services& services, dpp::snowflake guild_id, int page) {
    auto stmt = services.db.prepare(
        "SELECT id, kind, status, scanned, matched, actioned, started_by, created_at "
        "FROM jobs WHERE guild_id=?1 ORDER BY id DESC LIMIT 100");
    stmt.bind(1, static_cast<std::int64_t>(static_cast<std::uint64_t>(guild_id)));

    std::vector<std::string> lines;
    while (stmt.step()) {
        std::int64_t id = stmt.column_int(0);
        std::string kind = stmt.column_text(1);
        std::string status = stmt.column_text(2);
        std::int64_t scanned = stmt.column_int(3);
        std::int64_t matched = stmt.column_int(4);
        std::int64_t actioned = stmt.column_int(5);
        auto started_by = static_cast<std::uint64_t>(stmt.column_int(6));
        std::int64_t created = stmt.column_int(7);

        lines.push_back("`#" + std::to_string(id) + "` " + kind + " — " +
                        status_emoji(status) + " " + status + " · " + std::to_string(scanned) +
                        "/" + std::to_string(matched) + "/" + std::to_string(actioned) +
                        " · <@" + std::to_string(started_by) +
                        "> · <t:" + std::to_string(created) + ":R>");
    }

    PageView view = paginate(lines, page);
    std::string footer = "scanned / matched / actioned";
    if (view.pages > 1) {
        footer +=
            " · page " + std::to_string(view.page + 1) + "/" + std::to_string(view.pages);
    }

    dpp::embed embed;
    embed.set_title("Background jobs")
        .set_description(view.body.empty() ? "No jobs yet." : view.body)
        .set_footer(dpp::embed_footer().set_text(footer))
        .set_color(kEmbedColor);

    dpp::message msg = dpp::message().add_embed(embed).set_flags(dpp::m_ephemeral);
    if (view.pages > 1) msg.add_component(page_buttons("jobs", view.page, view.pages));
    return msg;
}

void handle_cancel(Services& services, const dpp::slashcommand_t& event,
                   const dpp::command_data_option& sub) {
    std::int64_t id = option_as<std::int64_t>(sub, "id").value_or(0);

    // Confirm the job belongs to this server before touching it.
    auto stmt = services.db.prepare("SELECT guild_id FROM jobs WHERE id=?1");
    stmt.bind(1, id);
    bool in_guild = stmt.step() && static_cast<std::uint64_t>(stmt.column_int(0)) ==
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

    dpp::command_option cancel(dpp::co_sub_command, "cancel",
                               "Cancel a queued or running job");
    cancel.add_option(
        dpp::command_option(dpp::co_integer, "id", "Job ID (from /jobs list)", true));
    sc.add_option(cancel);
    return sc;
}

void Jobs::handle(const dpp::slashcommand_t& event) const {
    if (!require_guild(event)) return;

    const auto& interaction = event.command.get_command_interaction();
    if (interaction.options.empty()) return;
    const auto& sub = interaction.options[0];

    if (sub.name == "list") {
        event.reply(list_message(*services_, event.command.guild_id, 0));
    } else if (sub.name == "cancel") {
        handle_cancel(*services_, event, sub);
    }
}

void Jobs::handle_button(const dpp::button_click_t& event) const {
    auto page = parse_page_custom_id(event.custom_id);
    if (!page) return;
    // The list is ephemeral, so only its owner can see (and click) it.
    event.reply(dpp::ir_update_message,
                list_message(*services_, event.command.guild_id, *page));
}

} // namespace broom::commands
