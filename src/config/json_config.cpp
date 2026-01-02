#include "broom_bot/config/json_config.h"

#include <fstream>

nlohmann::json broom_bot::config::read_json_file(const std::string& path,
                                                 std::string* error) {
    if (error) error->clear();

    std::ifstream f(path);
    if (!f.is_open()) {
        if (error) *error = "Could not open file: " + path;
        return nlohmann::json::object();
    }

    try {
        nlohmann::json j;
        f >> j;

        if (!j.is_object()) {
            if (error) *error = "Root JSON value must be an object: " + path;
            return nlohmann::json::object();
        }

        return j;
    } catch (const std::exception& e) {
        if (error) {
            *error = std::string("JSON parse error in ") + path + ": " + e.what();
        }
        return nlohmann::json::object();
    }
}
