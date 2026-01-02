#include "broom_bot/bot/commands.h"

#include <dpp/dpp.h>

#include <random>

namespace broom_bot::bot {

static bool coinflip() {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    static thread_local std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) == 1;
}

void register_handlers(dpp::cluster& bot, const broom_bot::config::BotConfig& cfg) {
    bot.on_slashcommand([](const dpp::slashcommand_t& e) {
        const auto name = e.command.get_command_name();

        if (name == "ping") {
            e.reply("Pong!");
            return;
        }

        if (name == "coinflip") {
            e.reply(coinflip() ? "Heads" : "Tails");
            return;
        }
    });

    bot.on_ready([&bot, &cfg](const dpp::ready_t&) {
        if (!dpp::run_once<struct register_bot_commands>()) return;

        dpp::slashcommand ping("ping", "Ping pong!", bot.me.id);
        dpp::slashcommand coin("coinflip", "Flip a coin", bot.me.id);

        if (cfg.dev_guild_id != 0) {
            const dpp::snowflake gid(cfg.dev_guild_id); // Register to guild for faster slash command propagation
            bot.guild_command_create(ping, gid);
            bot.guild_command_create(coin, gid);
        } else {
            bot.global_command_create(ping);
            bot.global_command_create(coin);
        }
    });
}

} // namespace broom_bot::bot
