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

// /remind set|list|cancel — personal reminders delivered as a mention in the
// channel where they were set.
//
// Deliberately NOT built on JobRunner: jobs are exclusive per guild (a pending
// reminder would block /purge, and a running purge would delay deliveries).
// Persistence + delivery live in ReminderService below.
class Remind : public Command {
public:
    explicit Remind(Services& services) : services_(&services) {}

    std::string name() const override { return "remind"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;

private:
    Services* services_;
};

// Polls the reminders table (~5s) and posts due reminders. One instance,
// owned by main; start() after the cluster is constructed. Delivery is
// at-most-once: a reminder is marked sent before the REST call fires, so a
// crash can drop a reminder but never duplicate it.
class ReminderService {
public:
    ReminderService(dpp::cluster& bot, Db& db);
    ~ReminderService();
    ReminderService(const ReminderService&) = delete;
    ReminderService& operator=(const ReminderService&) = delete;

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

// Schema steps for the reminders table; appended to the global migration list.
const std::vector<std::string>& remind_schema();

} // namespace broom::commands
