#include "commands/roll.hpp"

#include "core/rng.hpp"

#include <charconv>
#include <string>

namespace broom::commands {

namespace {

constexpr int max_count = 20;
constexpr int max_sides = 1000;

// Parses "NdM" (both parts optional: "d20", "3d6", "2"). Returns false on
// malformed input or out-of-range values.
bool parse_dice(const std::string& spec, int& count, int& sides) {
    count = 1;
    sides = 6;

    auto d = spec.find_first_of("dD");
    const char* end = spec.data() + spec.size();

    if (d == std::string::npos) {
        return spec.empty() || (std::from_chars(spec.data(), end, count).ptr == end &&
                                count >= 1 && count <= max_count);
    }

    const char* count_end = spec.data() + d;
    if (d > 0 && (std::from_chars(spec.data(), count_end, count).ptr != count_end ||
                  count < 1 || count > max_count)) {
        return false;
    }

    const char* sides_begin = spec.data() + d + 1;
    return sides_begin != end && std::from_chars(sides_begin, end, sides).ptr == end &&
           sides >= 2 && sides <= max_sides;
}

} // namespace

dpp::slashcommand Roll::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Roll dice", app_id)
        .add_option(dpp::command_option(dpp::co_string, "dice",
                                        "Dice to roll, e.g. 2d6 (default 1d6)", false));
}

void Roll::handle(const dpp::slashcommand_t& event) const {
    std::string spec;
    if (auto param = event.get_parameter("dice"); std::holds_alternative<std::string>(param)) {
        spec = std::get<std::string>(param);
    }

    int count = 0, sides = 0;
    if (!parse_dice(spec, count, sides)) {
        event.reply(dpp::message("Invalid dice spec `" + spec +
                                 "` - use NdM, e.g. `2d6` (max " + std::to_string(max_count) +
                                 " dice, " + std::to_string(max_sides) + " sides).")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    int total = 0;
    std::string rolls;
    for (int i = 0; i < count; ++i) {
        int r = rng_int(1, sides);
        total += r;
        if (i) rolls += ", ";
        rolls += std::to_string(r);
    }

    std::string reply = "🎲 " + std::to_string(count) + "d" + std::to_string(sides) + ": **" +
                        std::to_string(total) + "**";
    if (count > 1) reply += " (" + rolls + ")";
    event.reply(reply);
}

} // namespace broom::commands
