#include "commands/all_commands.hpp"
#include "core/config.hpp"
#include "core/db.hpp"
#include "core/jobs.hpp"
#include "core/registry.hpp"

#include <dpp/dpp.h>
#include <cstdio>
#include <exception>
#include <filesystem>

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
    db.migrate(broom::job_schema());

    dpp::cluster bot(config.bot_token);
    bot.on_log(dpp::utility::cout_logger());

    broom::CommandRegistry registry(broom::all_commands());
    registry.attach(bot, config.dev_guild_id);

    broom::JobRunner jobs(bot, db);
    // Job kinds are registered by the commands that use them (none yet).
    jobs.start();

    bot.start(dpp::st_wait);
    return 0;
}
