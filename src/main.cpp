#include <dpp/dpp.h>

#include <cstdio>

#include "broom_bot/config/json_config.h"

int main() {
    broom_bot::config::BotConfig cfg;
    std::string err;

    if (!broom_bot::config::load_bot_config(cfg, &err)) {
        if (!err.empty()) fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }

    dpp::cluster bot(cfg.bot_token);

    bot.on_slashcommand([](const dpp::slashcommand_t& e) {
        if (e.command.get_command_name() == "ping") e.reply("Pong!");
    });

    bot.on_ready([&bot](const dpp::ready_t&) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
    return 0;
}
