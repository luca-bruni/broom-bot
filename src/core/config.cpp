#include "core/config.hpp"

#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>

namespace broom {
namespace {

std::map<std::string, std::string> parse_env_file(const std::string& path) {
    std::map<std::string, std::string> values;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        values[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return values;
}

std::string get(const std::map<std::string, std::string>& file_values, const char* key) {
    if (const char* env = std::getenv(key)) return env;
    auto it = file_values.find(key);
    return it != file_values.end() ? it->second : "";
}

} // namespace

Config Config::load(const std::string& env_path) {
    auto file_values = parse_env_file(env_path);

    Config config;
    config.bot_token = get(file_values, "BOT_TOKEN");
    if (config.bot_token.empty()) {
        throw std::runtime_error(
            "BOT_TOKEN is not set (checked environment and " + env_path + ")");
    }

    if (std::string guild = get(file_values, "DEV_GUILD_ID"); !guild.empty()) {
        config.dev_guild_id = std::stoull(guild);
    }
    if (std::string dir = get(file_values, "DATA_DIR"); !dir.empty()) {
        config.data_dir = dir;
    }
    return config;
}

} // namespace broom
