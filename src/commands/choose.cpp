#include "commands/choose.hpp"

#include "core/rng.hpp"

#include <string>
#include <vector>

namespace broom::commands {

namespace {

std::vector<std::string> split_options(const std::string& input) {
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= input.size()) {
        auto sep = input.find('|', start);
        auto len = (sep == std::string::npos ? input.size() : sep) - start;
        std::string item = input.substr(start, len);

        auto first = item.find_first_not_of(" \t");
        auto last = item.find_last_not_of(" \t");
        if (first != std::string::npos) {
            items.push_back(item.substr(first, last - first + 1));
        }

        if (sep == std::string::npos) break;
        start = sep + 1;
    }
    return items;
}

} // namespace

dpp::slashcommand Choose::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Choose between options", app_id)
        .add_option(dpp::command_option(dpp::co_string, "options",
                                        "Options separated by |, e.g. pizza | sushi | tacos",
                                        true));
}

void Choose::handle(const dpp::slashcommand_t& event) const {
    auto items = split_options(std::get<std::string>(event.get_parameter("options")));

    if (items.size() < 2) {
        event.reply(dpp::message("Give me at least two options separated by `|`, "
                                 "e.g. `pizza | sushi | tacos`.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    event.reply(
        "🎲 I choose: **" +
        items[static_cast<std::size_t>(rng_int(0, static_cast<int>(items.size()) - 1))] +
        "**");
}

} // namespace broom::commands
