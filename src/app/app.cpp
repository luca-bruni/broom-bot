#include "broom_bot/app/app.h"

#include "broom_bot/bot/commands.h"
#include "broom_bot/config/json_config.h"

#include <dpp/dpp.h>

namespace broom_bot::app {

int run(const broom_bot::config::BotConfig& cfg) {
    dpp::cluster bot(cfg.bot_token);

    broom_bot::bot::register_handlers(bot, cfg);

    bot.start(dpp::st_wait);
    return 0;
}

} // namespace broom_bot::app
