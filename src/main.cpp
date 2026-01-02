#include <cstdio>
#include <string>

#include "broom_bot/app/app.h"
#include "broom_bot/config/json_config.h"


int main() {
    broom_bot::config::BotConfig cfg;
    std::string err;

    if (!broom_bot::config::load_bot_config(cfg, &err)) {
        if (!err.empty()) fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }

    return broom_bot::app::run(cfg);
}
