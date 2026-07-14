#include "commands/remind.hpp"

#include "commands/embeds.hpp"
#include "commands/options.hpp"
#include "commands/pagination.hpp"
#include "commands/remind_rules.hpp"
#include "core/db.hpp"
#include "core/duration.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace broom::commands {

namespace {

std::int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::int64_t to_i64(dpp::snowflake id) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(id));
}

void handle_set(Services& services, const dpp::slashcommand_t& event,
                const dpp::command_data_option& sub) {
    std::string in = option_as<std::string>(sub, "in").value_or("");
    std::string text = option_as<std::string>(sub, "message").value_or("");

    auto seconds = parse_duration_seconds(in);
    if (std::string error = validate_reminder(seconds, text); !error.empty()) {
        event.reply(dpp::message(error).set_flags(dpp::m_ephemeral));
        return;
    }

    auto pending = services.db.prepare(
        "SELECT COUNT(*) FROM reminders WHERE user_id=?1 AND status='pending'");
    pending.bind(1, to_i64(event.command.usr.id));
    pending.step();
    if (pending.column_int(0) >= kRemindMaxPendingPerUser) {
        event.reply(dpp::message("You already have " +
                                 std::to_string(kRemindMaxPendingPerUser) +
                                 " pending reminders — cancel one first (/remind list).")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    std::int64_t due_at = now_seconds() + *seconds;
    services.db
        .prepare(
            "INSERT INTO reminders(guild_id, channel_id, user_id, message, "
            "due_at, created_at) VALUES(?1,?2,?3,?4,?5,?6)")
        .bind(1, to_i64(event.command.guild_id))
        .bind(2, to_i64(event.command.channel_id))
        .bind(3, to_i64(event.command.usr.id))
        .bind(4, text)
        .bind(5, due_at)
        .bind(6, now_seconds())
        .step();
    std::int64_t id = services.db.last_insert_id();

    event.reply(dpp::message("⏰ Reminder `#" + std::to_string(id) +
                             "` set — I'll ping "
                             "you here <t:" +
                             std::to_string(due_at) + ":R>.")
                    .set_flags(dpp::m_ephemeral));
}

// Shared by /remind list and its pagination buttons.
dpp::message list_message(Services& services, dpp::snowflake user_id, int page) {
    auto stmt = services.db.prepare(
        "SELECT id, message, due_at FROM reminders WHERE user_id=?1 AND "
        "status='pending' ORDER BY due_at LIMIT 100");
    stmt.bind(1, to_i64(user_id));

    std::vector<std::string> lines;
    while (stmt.step()) {
        lines.push_back("`#" + std::to_string(stmt.column_int(0)) + "` <t:" +
                        std::to_string(stmt.column_int(2)) + ":R> — " + stmt.column_text(1));
    }

    PageView view = paginate(lines, page);
    dpp::embed embed;
    embed.set_title("Your reminders")
        .set_description(view.body.empty() ? "No pending reminders." : view.body)
        .set_color(kEmbedColor);
    if (view.pages > 1) {
        embed.set_footer(dpp::embed_footer().set_text("Page " + std::to_string(view.page + 1) +
                                                      "/" + std::to_string(view.pages)));
    }

    dpp::message msg = dpp::message().add_embed(embed).set_flags(dpp::m_ephemeral);
    if (view.pages > 1) msg.add_component(page_buttons("remind", view.page, view.pages));
    return msg;
}

void handle_cancel(Services& services, const dpp::slashcommand_t& event,
                   const dpp::command_data_option& sub) {
    std::int64_t id = option_as<std::int64_t>(sub, "id").value_or(0);

    // Only the owner may cancel, and only while still pending.
    auto stmt = services.db.prepare(
        "UPDATE reminders SET status='cancelled' WHERE id=?1 AND user_id=?2 AND "
        "status='pending' RETURNING id");
    stmt.bind(1, id).bind(2, to_i64(event.command.usr.id));
    bool cancelled = stmt.step();

    event.reply(
        dpp::message(cancelled ? "🛑 Reminder `#" + std::to_string(id) + "` cancelled."
                               : "No pending reminder `#" + std::to_string(id) + "` of yours.")
            .set_flags(dpp::m_ephemeral));
}

} // namespace

dpp::slashcommand Remind::definition(dpp::snowflake app_id) const {
    dpp::slashcommand sc(name(), "Set, list, or cancel personal reminders", app_id);

    dpp::command_option set(dpp::co_sub_command, "set", "Set a reminder");
    set.add_option(dpp::command_option(
        dpp::co_string, "in", "When, e.g. 20m, 2h, 1d (units: s m h d w mo y)", true));
    set.add_option(
        dpp::command_option(dpp::co_string, "message", "What to remind you about", true));
    sc.add_option(set);

    sc.add_option(
        dpp::command_option(dpp::co_sub_command, "list", "List your pending reminders"));

    dpp::command_option cancel(dpp::co_sub_command, "cancel", "Cancel a reminder");
    cancel.add_option(
        dpp::command_option(dpp::co_integer, "id", "Reminder ID (from /remind list)", true));
    sc.add_option(cancel);
    return sc;
}

void Remind::handle(const dpp::slashcommand_t& event) const {
    if (!require_guild(event)) return;

    const auto& interaction = event.command.get_command_interaction();
    if (interaction.options.empty()) return;
    const auto& sub = interaction.options[0];

    if (sub.name == "set") {
        handle_set(*services_, event, sub);
    } else if (sub.name == "list") {
        event.reply(list_message(*services_, event.command.usr.id, 0));
    } else if (sub.name == "cancel") {
        handle_cancel(*services_, event, sub);
    }
}

void Remind::handle_button(const dpp::button_click_t& event) const {
    auto page = parse_page_custom_id(event.custom_id);
    if (!page) return;
    // The list is ephemeral, so only its owner can see (and click) it.
    event.reply(dpp::ir_update_message, list_message(*services_, event.command.usr.id, *page));
}

const std::vector<std::string>& remind_schema() {
    // Append-only. New steps go at the end (see main's migration list).
    static const std::vector<std::string> steps = {
        R"sql(
        CREATE TABLE reminders(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            channel_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            message TEXT NOT NULL,
            due_at INTEGER NOT NULL,
            status TEXT NOT NULL DEFAULT 'pending',
            created_at INTEGER NOT NULL
        );
        CREATE INDEX idx_reminders_due ON reminders(status, due_at);
        CREATE INDEX idx_reminders_user ON reminders(user_id, status);
        )sql",
    };
    return steps;
}

} // namespace broom::commands
