#include "broom_bot/config/json_config.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>

nlohmann::json broom_bot::config::read_json_file(const std::string& path,
                                                 std::string* error) {
    if (error)
        error->clear();

    std::ifstream f(path);
    if (!f.is_open()) {
        if (error)
            *error = "Could not open file: " + path;

        return nlohmann::json::object();
    }

    try {
        nlohmann::json j;
        f >> j;

        if (!j.is_object()) {
            if (error)
                *error = "Root JSON value must be an object: " + path;

            return nlohmann::json::object();
        }

        return j;
    } catch (const std::exception& e) {
        if (error)
            *error = std::string("JSON parse error in ") + path + ": " + e.what();

        return nlohmann::json::object();
    }
}

bool broom_bot::config::load_bot_config(BotConfig& out,
                                        std::string* error,
                                        const std::string& path) {
    if (error)
        error->clear();

    out = BotConfig{};

    std::string parse_err;
    nlohmann::json cfg = read_json_file(path, &parse_err);

    if (cfg.empty()) {
        if (error) {
            if (!parse_err.empty()) {
                *error = parse_err + "\nExpected config.json in the current working directory.";
            } else {
                *error = "Expected config.json in the current working directory.";
            }
        }
        return false;
    }

    out.bot_token = cfg.value("BOT_TOKEN", "");
    if (out.bot_token.empty()) {
        if (error)
            *error = "Missing BOT_TOKEN in config.json.";

        return false;
    }

    out.dev_guild_id = 0;
    if (cfg.contains("DEV_GUILD_ID")) {
        try {
            if (cfg["DEV_GUILD_ID"].is_string()) {
                out.dev_guild_id = std::stoull(cfg["DEV_GUILD_ID"].get<std::string>());
            } else if (cfg["DEV_GUILD_ID"].is_number_unsigned()) {
                out.dev_guild_id = cfg["DEV_GUILD_ID"].get<std::uint64_t>();
            } else if (error) {
                *error = "DEV_GUILD_ID must be a string or an unsigned number.";
                return false;
            }
        } catch (...) {
            if (error) *error = "DEV_GUILD_ID is not a valid Discord server ID.";
            return false;
        }
    }

    return true;
}
