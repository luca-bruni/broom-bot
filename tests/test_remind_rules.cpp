#include "doctest.h"

#include "commands/remind_rules.hpp"
#include "core/duration.hpp"

#include <string>

using broom::parse_duration_seconds;
using namespace broom::commands;

static std::string check(const std::string& in, const std::string& text) {
    return validate_reminder(parse_duration_seconds(in), text);
}

TEST_CASE("validate_reminder: accepts sane input") {
    CHECK(check("20m", "stretch").empty());
    CHECK(check("10s", "minimum boundary").empty());
    CHECK(check("2h30m", "combined units").empty());
    CHECK(check("1y", "long but allowed").empty());
    CHECK(validate_reminder(kRemindMaxSeconds, "max boundary").empty());
}

TEST_CASE("validate_reminder: rejects bad durations") {
    CHECK_FALSE(check("", "text").empty());     // unparseable
    CHECK_FALSE(check("soon", "text").empty()); // unparseable
    CHECK_FALSE(check("5s", "text").empty());   // below minimum
    CHECK_FALSE(check("3y", "text").empty());   // above maximum
    CHECK_FALSE(validate_reminder(kRemindMaxSeconds + 1, "text").empty());
}

TEST_CASE("validate_reminder: rejects bad messages") {
    CHECK_FALSE(check("20m", "").empty()); // empty message
    CHECK_FALSE(check("20m", std::string(kRemindMaxLength + 1, 'x')).empty());
    CHECK(check("20m", std::string(kRemindMaxLength, 'x')).empty()); // boundary ok
}

TEST_CASE("validate_reminder: duration errors take priority over message errors") {
    // Both invalid -> the duration error is reported first.
    auto error = check("nope", "");
    CHECK(error.find("Invalid `in`") == 0);
}
