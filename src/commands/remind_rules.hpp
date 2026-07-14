#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace broom::commands {

// Pure validation rules for /remind, kept DPP-free so they can be unit-tested.
// See tests/test_remind_rules.cpp.

inline constexpr std::int64_t kRemindMinSeconds = 10;
inline constexpr std::int64_t kRemindMaxSeconds = 2LL * 365 * 86400; // 2 years
inline constexpr std::size_t kRemindMaxLength = 500;
inline constexpr std::int64_t kRemindMaxPendingPerUser = 25;

// Empty string = valid; otherwise a user-facing error message.
// `seconds` is the parsed duration (nullopt when parsing failed).
inline std::string validate_reminder(std::optional<std::int64_t> seconds,
                                     const std::string& text) {
    if (!seconds) {
        return "Invalid `in` — use a duration like `20m`, `2h`, or `1d` "
               "(units: s m h d w mo y).";
    }
    if (*seconds < kRemindMinSeconds) {
        return "That's too soon — the minimum is " + std::to_string(kRemindMinSeconds) +
               " seconds.";
    }
    if (*seconds > kRemindMaxSeconds) {
        return "That's too far out — the maximum is 2 years.";
    }
    if (text.empty()) return "The reminder message can't be empty.";
    if (text.size() > kRemindMaxLength) {
        return "Reminder message too long — keep it under " +
               std::to_string(kRemindMaxLength) + " characters.";
    }
    return "";
}

} // namespace broom::commands
