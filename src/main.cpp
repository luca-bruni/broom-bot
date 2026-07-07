#include "commands/all_commands.hpp"
#include "commands/purge.hpp"
#include "core/config.hpp"
#include "core/db.hpp"
#include "core/jobs.hpp"
#include "core/registry.hpp"
#include "core/services.hpp"

#include <dpp/dpp.h>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <vector>

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
    db.migrate(migrations);

    dpp::cluster bot(config.bot_token);
    bot.on_log(dpp::utility::cout_logger());

    broom::JobRunner jobs(bot, db);
    broom::Services services{jobs, db};

    broom::CommandRegistry registry(broom::all_commands(services));
    broom::commands::register_purge_jobs(jobs, db);
    registry.attach(bot, config.dev_guild_id);
    jobs.start();

    bot.start(dpp::st_wait);
    return 0;
}
