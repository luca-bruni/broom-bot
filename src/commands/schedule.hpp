#pragma once

#include "core/command.hpp"
#include "core/services.hpp"

#include <dpp/dpp.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace broom {
class Db;
}

namespace broom::commands {

// /schedule message|event - mod-gated scheduling (Manage Server).
//   message: the bot posts the text in a channel at the given time (rides the
//            same timed-delivery table as /remind, kind='message').
//   event:   creates a native Discord scheduled event (external/location type)
//            immediately - Discord itself handles the countdown, so no row is
//            stored for these.
class Schedule : public Command {
public:
    explicit Schedule(Services& services) : services_(&services) {}

    std::string name() const override { return "schedule"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;

private:
    Services* services_;
};

// Timed delivery for everything in the reminders table, dispatched by kind:
//   'reminder' → "⏰ @user Reminder: <text>"   (set by /remind)
//   'message'  → "<text>"                      (set by /schedule message)
//
// One polling thread (~5s), owned by main, RAII shutdown. Deliberately NOT
// JobRunner-based: jobs are exclusive per guild, so a pending item would block
// /purge and a running purge would delay deliveries. At-most-once: rows are
// marked sent before the REST call, so a crash drops rather than duplicates.
class ScheduleService {
public:
    ScheduleService(dpp::cluster& bot, Db& db);
    ~ScheduleService();
    ScheduleService(const ScheduleService&) = delete;
    ScheduleService& operator=(const ScheduleService&) = delete;

    void start();

private:
    void loop();

    dpp::cluster& bot_;
    Db& db_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
    std::thread worker_;
};

// Schema steps for /schedule (extends the reminders table with a kind);
// appended to the global migration list after remind_schema().
const std::vector<std::string>& schedule_schema();

} // namespace broom::commands
