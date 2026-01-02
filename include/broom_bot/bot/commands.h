#pragma once

#include "broom_bot/config/json_config.h"

namespace dpp {
class cluster;
}

namespace broom_bot::bot {

/*
 * Register all event handlers and commands here.
 */
void register_handlers(dpp::cluster& bot, const broom_bot::config::BotConfig& cfg);

} // namespace broom_bot::bot
