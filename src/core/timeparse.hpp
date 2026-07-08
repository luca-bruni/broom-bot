#pragma once

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

#ifdef _WIN32
#define timegm _mkgmtime
#endif

namespace broom {

inline constexpr std::uint64_t kDiscordEpochMs = 1420070400000ULL;

// Discord snowflake for a given unix-ms timestamp (0 if before the epoch).
inline std::uint64_t ms_to_snowflake(std::uint64_t ms) {
    return ms <= kDiscordEpochMs ? 0 : ((ms - kDiscordEpochMs) << 22);
}

// Parses YYYY-MM-DD as UTC into unix-ms. end_of_day pushes to 23:59:59.999.
// Returns false on malformed input or out-of-range month/day.
inline bool parse_date_ms(const std::string& s, bool end_of_day, std::uint64_t& out_ms) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = end_of_day ? 23 : 0;
    tm.tm_min = end_of_day ? 59 : 0;
    tm.tm_sec = end_of_day ? 59 : 0;
    std::time_t t = timegm(&tm);
    if (t == static_cast<std::time_t>(-1)) return false;
    out_ms = static_cast<std::uint64_t>(t) * 1000 + (end_of_day ? 999 : 0);
    return true;
}

// "~3s" / "~5m" / "~2h 10m" for a rough duration display.
inline std::string human_duration(std::int64_t seconds) {
    if (seconds < 60) return "~" + std::to_string(seconds) + "s";
    if (seconds < 3600) return "~" + std::to_string(seconds / 60) + "m";
    return "~" + std::to_string(seconds / 3600) + "h " +
           std::to_string((seconds % 3600) / 60) + "m";
}

} // namespace broom
