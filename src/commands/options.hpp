#pragma once

#include <dpp/dpp.h>

#include <optional>
#include <string_view>

namespace broom::commands {

// Typed lookup of a named option on a subcommand (or any command_data_option
// with nested options). Returns nullopt when the option is absent or holds a
// different type - combine with value_or() for defaults:
//
//   auto keywords = option_as<std::string>(sub, "keywords").value_or("");
//   bool bots     = option_as<bool>(sub, "bots_only").value_or(false);
template <typename T>
std::optional<T> option_as(const dpp::command_data_option& parent, std::string_view name) {
    for (const auto& option : parent.options) {
        if (option.name == name && std::holds_alternative<T>(option.value)) {
            return std::get<T>(option.value);
        }
    }
    return std::nullopt;
}

} // namespace broom::commands
