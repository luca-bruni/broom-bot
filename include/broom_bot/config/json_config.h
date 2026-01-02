#pragma once

#include <string>

#include <dpp/nlohmann/json.hpp>

namespace broom_bot::config {

nlohmann::json read_json_file(const std::string& path,
                              std::string* error = nullptr);

} // namespace broom_bot::config
