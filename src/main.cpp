#include "commands/all_commands.hpp"
#include "core/config.hpp"
#include "core/registry.hpp"

#include <dpp/dpp.h>
#include <cstdio>
#include <exception>

int main() {
    broom::Config config;
    try {
        config = broom::Config::load();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Config error: %s\n", e.what());
        return 1;
    }

    dpp::cluster bot(config.bot_token);
    bot.on_log(dpp::utility::cout_logger());

    broom::CommandRegistry registry(broom::all_commands());
    registry.attach(bot, config.dev_guild_id);

    bot.start(dpp::st_wait);
    return 0;
}
