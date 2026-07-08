#pragma once

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

namespace broom {

// Parses a human duration like "30s", "15m", "2h", "3d", "2w", "6mo", "1y",
// or concatenations ("1w2d", "1mo15d"). Units: s m h d w mo y (mo=30d, y=365d).
// Returns total seconds, or nullopt if malformed/empty. int64 seconds covers
// spans far beyond any realistic reminder (billions of years).
inline std::optional<std::int64_t> parse_duration_seconds(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::int64_t total = 0;
    std::size_t i = 0;
    while (i < s.size()) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
        std::int64_t value = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            value = value * 10 + (s[i] - '0');
            ++i;
        }
        std::int64_t unit = 0;
        if (i + 1 < s.size() && s[i] == 'm' && s[i + 1] == 'o') {
            unit = 2592000; // 30 days
            i += 2;
        } else if (i < s.size()) {
            switch (s[i]) {
                case 's': unit = 1; break;
                case 'm': unit = 60; break;
                case 'h': unit = 3600; break;
                case 'd': unit = 86400; break;
                case 'w': unit = 604800; break;
                case 'y': unit = 31536000; break; // 365 days
                default: return std::nullopt;
            }
            ++i;
        } else {
            return std::nullopt; // trailing number with no unit
        }
        total += value * unit;
    }
    return total;
}

} // namespace broom
