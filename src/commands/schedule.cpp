#include "commands/schedule.hpp"

#include "commands/embeds.hpp"
#include "commands/options.hpp"
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

constexpr std::int64_t kEventMinDurationSeconds = 60;
constexpr std::int64_t kEventMaxDurationSeconds = 7LL * 86400; // one week

void handle_message(Services& services, const dpp::slashcommand_t& event,
                    const dpp::command_data_option& sub) {
    std::string in = option_as<std::string>(sub, "in").value_or("");
    std::string text = option_as<std::string>(sub, "message").value_or("");
    dpp::snowflake channel =
        option_as<dpp::snowflake>(sub, "channel").value_or(event.command.channel_id);

    auto seconds = parse_duration_seconds(in);
    if (std::string error = validate_reminder(seconds, text); !error.empty()) {
        event.reply(dpp::message(error).set_flags(dpp::m_ephemeral));
        return;
    }

    std::int64_t due_at = now_seconds() + *seconds;
    services.db
        .prepare(
            "INSERT INTO reminders(guild_id, channel_id, user_id, message, "
            "due_at, created_at, kind) VALUES(?1,?2,?3,?4,?5,?6,'message')")
        .bind(1, to_i64(event.command.guild_id))
        .bind(2, to_i64(channel))
        .bind(3, to_i64(event.command.usr.id))
        .bind(4, text)
        .bind(5, due_at)
        .bind(6, now_seconds())
        .step();
    std::int64_t id = services.db.last_insert_id();

    event.reply(dpp::message("📨 Scheduled message `#" + std::to_string(id) +
                             "` - posts in <#" + std::to_string(channel) +
                             "> <t:" + std::to_string(due_at) + ":R>.")
                    .set_flags(dpp::m_ephemeral));
}

void handle_event(Services&, const dpp::slashcommand_t& event,
                  const dpp::command_data_option& sub) {
    std::string name = option_as<std::string>(sub, "name").value_or("");
    std::string in = option_as<std::string>(sub, "in").value_or("");
    std::string location = option_as<std::string>(sub, "location").value_or("");
    std::string duration_raw = option_as<std::string>(sub, "duration").value_or("1h");

    auto start_in = parse_duration_seconds(in);
    if (std::string error = validate_reminder(start_in, name); !error.empty()) {
        event.reply(dpp::message(error).set_flags(dpp::m_ephemeral));
        return;
    }
    if (name.size() > 100 || location.empty() || location.size() > 100) {
        event.reply(dpp::message("`name` and `location` must be 1–100 characters.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }
    auto duration = parse_duration_seconds(duration_raw);
    if (!duration || *duration < kEventMinDurationSeconds ||
        *duration > kEventMaxDurationSeconds) {
        event.reply(dpp::message("Invalid `duration` - between `1m` and `7d`, e.g. `90m`.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    dpp::scheduled_event ev;
    ev.guild_id = event.command.guild_id;
    ev.name = name;
    ev.entity_type = dpp::eet_external;
    ev.privacy_level = dpp::ep_guild_only;
    ev.scheduled_start_time = static_cast<time_t>(now_seconds() + *start_in);
    ev.scheduled_end_time = static_cast<time_t>(now_seconds() + *start_in + *duration);
    ev.set_location(location);

    // The REST call outlives the 3s ack window; ack first, edit when done.
    event.thinking(true, [event, ev](const dpp::confirmation_callback_t&) {
        event.owner->guild_event_create(
            ev, [event, ev](const dpp::confirmation_callback_t& cb) {
                if (cb.is_error()) {
                    event.edit_original_response(
                        dpp::message("⚠️ Couldn't create the event: " + cb.get_error().message +
                                     " (does the bot have Manage Events?)"));
                    return;
                }
                event.edit_original_response(
                    dpp::message("📅 Event **" + ev.name + "** created - starts <t:" +
                                 std::to_string(ev.scheduled_start_time) + ":R>."));
            });
    });
}

} // namespace

dpp::slashcommand Schedule::definition(dpp::snowflake app_id) const {
    dpp::slashcommand sc(name(), "Schedule a bot message or a Discord event", app_id);
    sc.set_default_permissions(dpp::p_manage_guild);

    dpp::command_option message(dpp::co_sub_command, "message",
                                "Post a message as the bot at a future time");
    message.add_option(dpp::command_option(
        dpp::co_string, "in", "When, e.g. 20m, 2h, 1d (units: s m h d w mo y)", true));
    message.add_option(
        dpp::command_option(dpp::co_string, "message", "What the bot should post", true));
    message.add_option(dpp::command_option(dpp::co_channel, "channel",
                                           "Where to post (default: here)", false));
    sc.add_option(message);

    dpp::command_option ev(dpp::co_sub_command, "event",
                           "Create a Discord scheduled event (external location)");
    ev.add_option(dpp::command_option(dpp::co_string, "name", "Event name", true));
    ev.add_option(dpp::command_option(dpp::co_string, "in",
                                      "Starts in, e.g. 2h, 3d (units: s m h d w mo y)", true));
    ev.add_option(
        dpp::command_option(dpp::co_string, "location", "Where it happens (free text)", true));
    ev.add_option(dpp::command_option(dpp::co_string, "duration",
                                      "How long it runs (default 1h)", false));
    sc.add_option(ev);
    return sc;
}

void Schedule::handle(const dpp::slashcommand_t& event) const {
    if (!require_guild(event)) return;

    const auto& interaction = event.command.get_command_interaction();
    if (interaction.options.empty()) return;
    const auto& sub = interaction.options[0];

    if (sub.name == "message") {
        handle_message(*services_, event, sub);
    } else if (sub.name == "event") {
        handle_event(*services_, event, sub);
    }
}

// --- ScheduleService ---------------------------------------------------------

ScheduleService::ScheduleService(dpp::cluster& bot, Db& db) : bot_(bot), db_(db) {}

ScheduleService::~ScheduleService() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void ScheduleService::start() {
    worker_ = std::thread([this] { loop(); });
}

void ScheduleService::loop() {
    for (;;) {
        {
            std::unique_lock lock(mutex_);
            wake_.wait_for(lock, std::chrono::seconds(5), [this] { return stopping_; });
            if (stopping_) return;
        }

        struct Due {
            std::int64_t id;
            std::string kind;
            std::int64_t channel_id;
            std::int64_t user_id;
            std::string message;
        };
        std::vector<Due> due;
        {
            auto stmt = db_.prepare(
                "SELECT id, kind, channel_id, user_id, message FROM reminders WHERE "
                "status='pending' AND due_at <= ?1 ORDER BY due_at LIMIT 20");
            stmt.bind(1, now_seconds());
            while (stmt.step()) {
                due.push_back({stmt.column_int(0), stmt.column_text(1), stmt.column_int(2),
                               stmt.column_int(3), stmt.column_text(4)});
            }
        }

        for (const auto& r : due) {
            // Mark sent first: a crash mid-send drops the item rather than
            // duplicating it on restart.
            db_.prepare("UPDATE reminders SET status='sent' WHERE id=?1").bind(1, r.id).step();
            std::string content =
                r.kind == "message"
                    ? r.message
                    : "⏰ <@" + std::to_string(static_cast<std::uint64_t>(r.user_id)) +
                          "> Reminder: " + r.message;
            bot_.message_create(dpp::message(
                dpp::snowflake(static_cast<std::uint64_t>(r.channel_id)), content));
        }
    }
}

const std::vector<std::string>& schedule_schema() {
    // Append-only. New steps go at the end (see main's migration list).
    static const std::vector<std::string> steps = {
        R"sql(
        ALTER TABLE reminders ADD COLUMN kind TEXT NOT NULL DEFAULT 'reminder';
        )sql",
    };
    return steps;
}

} // namespace broom::commands
