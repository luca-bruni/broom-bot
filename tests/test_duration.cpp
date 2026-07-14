#include "doctest.h"

#include "core/duration.hpp"

using broom::parse_duration_seconds;

TEST_CASE("parse_duration_seconds: single units") {
    CHECK(parse_duration_seconds("30s") == 30);
    CHECK(parse_duration_seconds("15m") == 15 * 60);
    CHECK(parse_duration_seconds("2h") == 2 * 3600);
    CHECK(parse_duration_seconds("3d") == 3 * 86400);
    CHECK(parse_duration_seconds("2w") == 2 * 604800);
    CHECK(parse_duration_seconds("6mo") == 6 * 2592000);
    CHECK(parse_duration_seconds("1y") == 31536000);
}

TEST_CASE("parse_duration_seconds: mo vs m disambiguation") {
    CHECK(parse_duration_seconds("1m") == 60);
    CHECK(parse_duration_seconds("1mo") == 2592000);
    CHECK(parse_duration_seconds("1m1mo") == 60 + 2592000);
}

TEST_CASE("parse_duration_seconds: combined tokens") {
    CHECK(parse_duration_seconds("1w2d") == 604800 + 2 * 86400);
    CHECK(parse_duration_seconds("1mo15d") == 2592000 + 15 * 86400);
    CHECK(parse_duration_seconds("1h30m") == 3600 + 1800);
    CHECK(parse_duration_seconds("0s") == 0);
}

TEST_CASE("parse_duration_seconds: invalid input returns nullopt") {
    CHECK_FALSE(parse_duration_seconds("").has_value());
    CHECK_FALSE(parse_duration_seconds("abc").has_value());
    CHECK_FALSE(parse_duration_seconds("10").has_value());  // no unit
    CHECK_FALSE(parse_duration_seconds("10x").has_value()); // bad unit
    CHECK_FALSE(parse_duration_seconds("m").has_value());   // no number
    CHECK_FALSE(parse_duration_seconds("1mox").has_value());
    CHECK_FALSE(parse_duration_seconds("1d ").has_value()); // trailing space
}

TEST_CASE("parse_duration_seconds: large spans (years) fit in int64") {
    // 10 years of seconds is nowhere near int64 max.
    CHECK(parse_duration_seconds("10y") == 10LL * 31536000);
}

TEST_CASE("parse_duration_seconds: overflow rejected, not wrapped") {
    // More digits than int64 can hold.
    CHECK_FALSE(parse_duration_seconds("99999999999999999999s").has_value());
    // Digits fit, but value*unit overflows.
    CHECK_FALSE(parse_duration_seconds("9223372036854775807y").has_value());
    // Each token fits, but the sum overflows.
    CHECK_FALSE(parse_duration_seconds("9223372036854775807s1s").has_value());
    // Near the limit but valid stays valid.
    CHECK(parse_duration_seconds("9223372036854775807s") == 9223372036854775807LL);
}
