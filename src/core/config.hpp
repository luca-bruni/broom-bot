#pragma once

#include <dpp/dpp.h>
#include <string>

namespace broom {

struct Config {
    std::string bot_token;
    dpp::snowflake dev_guild_id{0}; // 0 = register commands globally

    // Reads KEY=VALUE pairs from env_path (a missing file is fine), then
    // overlays real environment variables (environment wins).
    // Throws std::runtime_error if BOT_TOKEN ends up unset.
    static Config load(const std::string& env_path = ".env");
};

} // namespace broom
