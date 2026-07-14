#include "commands/all_commands.hpp"
#include "commands/purge.hpp"
#include "commands/remind.hpp"
#include "commands/schedule.hpp"
#include "core/catalog.hpp"
#include "core/config.hpp"
#include "core/db.hpp"
#include "core/jobs.hpp"
#include "core/metrics.hpp"
#include "core/registry.hpp"
#include "core/services.hpp"

#include <dpp/dpp.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <thread>
#include <vector>

#ifndef BROOM_VERSION
#define BROOM_VERSION "dev"
#endif

namespace {

// Graceful shutdown: the signal handler only sets a flag (async-signal-safe);
// a watcher thread calls cluster::shutdown(), which makes bot.start(st_wait)
// return so services destruct cleanly (worker threads join, DB closes).
std::atomic<bool> g_stop{false};

extern "C" void on_signal(int) { g_stop.store(true); }

} // namespace

int main() {
    broom::Config config;
    try {
        config = broom::Config::load();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Config error: %s\n", e.what());
        return 1;
    }

    std::filesystem::create_directories(config.data_dir);
    broom::Db db(config.data_dir + "/broom.db");

    // Single global, append-only migration list (schema versioning is one
    // PRAGMA user_version counter). NEVER reorder or insert in the middle —
    // append new steps at the end, even core ones.
    std::vector<std::string> migrations = broom::job_schema();
    const auto& purge_steps = broom::commands::purge_schema();
    migrations.insert(migrations.end(), purge_steps.begin(), purge_steps.end());
    const auto& remind_steps = broom::commands::remind_schema();
    migrations.insert(migrations.end(), remind_steps.begin(), remind_steps.end());
    const auto& schedule_steps = broom::commands::schedule_schema();
    migrations.insert(migrations.end(), schedule_steps.begin(), schedule_steps.end());
    const auto& metrics_steps = broom::metrics_schema();
    migrations.insert(migrations.end(), metrics_steps.begin(), metrics_steps.end());
    db.migrate(migrations);

    dpp::cluster bot(config.bot_token);
    bot.on_log(dpp::utility::cout_logger());

    broom::JobRunner jobs(bot, db);
    broom::CommandCatalog catalog;
    broom::Services services{jobs, db, catalog, std::chrono::steady_clock::now(),
                             BROOM_VERSION};

    auto commands = broom::all_commands(services);
    // Snapshot each command's name/description for /help and /stats (app_id is
    // irrelevant to those two fields, so 0 is fine here).
    for (const auto& command : commands) {
        dpp::slashcommand def = command->definition(0);
        catalog.entries.push_back({def.name, def.description});
    }

    broom::CommandRegistry registry(std::move(commands));
    registry.set_usage_hook(
        [&db](const std::string& name) { broom::record_command_use(db, name); });
    broom::commands::register_purge_jobs(jobs, db);
    registry.attach(bot, config.dev_guild_id);
    jobs.start();

    broom::commands::ScheduleService schedule(bot, db);
    schedule.start();

    // Presence: alternate between "Watching N servers" and "Listening to
    // /help" once a minute (also sets the initial status on ready).
    bot.on_ready([&bot](const dpp::ready_t&) {
        if (!dpp::run_once<struct set_presence>()) return;
        auto set_status = [&bot](bool watching) {
            if (watching) {
                std::size_t guilds =
                    dpp::get_guild_cache() ? dpp::get_guild_cache()->count() : 0;
                bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_watching,
                                               std::to_string(guilds) + " servers"));
            } else {
                bot.set_presence(dpp::presence(dpp::ps_online, dpp::at_listening, "/help"));
            }
        };
        set_status(true);
        bot.start_timer(
            [&bot, set_status](dpp::timer) {
                static std::atomic<bool> watching{false};
                set_status(watching.exchange(!watching.load()));
            },
            60);
    });

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
#ifdef SIGBREAK
    // Windows delivers Ctrl+Break / console-close as SIGBREAK, not SIGINT.
    std::signal(SIGBREAK, on_signal);
#endif
    std::thread shutdown_watcher([&bot] {
        while (!g_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        bot.log(dpp::ll_info, "Signal received — shutting down cleanly");
        bot.shutdown();
    });

    bot.start(dpp::st_wait);

    // start() returned (shutdown() or gateway exit): release the watcher and
    // fall off main so JobRunner/ScheduleService join their workers.
    g_stop.store(true);
    shutdown_watcher.join();
    return 0;
}
