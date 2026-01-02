#pragma once

#include <string>

#include <dpp/nlohmann/json.hpp>

namespace broom_bot::config {

struct BotConfig {
    std::string bot_token;
};

/*
 * Read arbitrary JSON file
 */
nlohmann::json read_json_file(const std::string& path,
                              std::string* error = nullptr);


/*
 * Parse cwd-adjacent config.json into caller's BotConfig
 */
bool load_bot_config(BotConfig& out,
                     std::string* error = nullptr,
                     const std::string& path = "config.json");

} // namespace broom_bot::config
