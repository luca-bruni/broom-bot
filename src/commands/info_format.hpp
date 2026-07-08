#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <regex>
#include <string>

namespace broom::commands {

// Pure formatting/parsing helpers for the info commands, kept DPP-free so they
// can be unit-tested. See tests/test_info_format.cpp.

// RGB integer (0xRRGGBB) to "#RRGGBB".
inline std::string format_color(std::uint32_t rgb) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%06X", rgb & 0xFFFFFFu);
    return buf;
}

// Human name for a Discord channel type value (dpp::channel_type numbers).
inline std::string channel_type_name(int type) {
    switch (type) {
        case 0: return "Text";
        case 1: return "DM";
        case 2: return "Voice";
        case 3: return "Group DM";
        case 4: return "Category";
        case 5: return "Announcement";
        case 10: return "Announcement Thread";
        case 11: return "Public Thread";
        case 12: return "Private Thread";
        case 13: return "Stage";
        case 14: return "Directory";
        case 15: return "Forum";
        case 16: return "Media";
        default: return "Unknown";
    }
}

struct CustomEmoji {
    bool animated = false;
    std::string name;
    std::uint64_t id = 0;
};

// Parses a custom emoji mention "<:name:id>" or animated "<a:name:id>".
// Returns nullopt for unicode emoji or malformed input.
inline std::optional<CustomEmoji> parse_custom_emoji(const std::string& s) {
    static const std::regex re(R"(^\s*<(a?):([A-Za-z0-9_]{2,32}):([0-9]+)>\s*$)");
    std::smatch m;
    if (!std::regex_match(s, m, re)) return std::nullopt;
    CustomEmoji e;
    e.animated = m[1].matched && m[1].length() > 0;
    e.name = m[2].str();
    e.id = std::stoull(m[3].str());
    return e;
}

// CDN URL for a custom emoji.
inline std::string emoji_url(std::uint64_t id, bool animated) {
    return "https://cdn.discordapp.com/emojis/" + std::to_string(id) +
           (animated ? ".gif" : ".png") + "?size=256";
}

} // namespace broom::commands
