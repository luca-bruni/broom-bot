#include "doctest.h"

#include "core/timeparse.hpp"

using namespace broom;

TEST_CASE("ms_to_snowflake: epoch boundary and shift") {
    CHECK(ms_to_snowflake(0) == 0);
    CHECK(ms_to_snowflake(kDiscordEpochMs) == 0);
    CHECK(ms_to_snowflake(kDiscordEpochMs + 1) == (1ULL << 22));
    CHECK(ms_to_snowflake(kDiscordEpochMs + 1000) == (1000ULL << 22));
}

TEST_CASE("parse_date_ms: valid dates (UTC)") {
    std::uint64_t ms = 0;
    REQUIRE(parse_date_ms("2021-01-01", false, ms));
    CHECK(ms == 1609459200000ULL); // 2021-01-01T00:00:00Z

    REQUIRE(parse_date_ms("2021-01-01", true, ms));
    CHECK(ms == 1609459200000ULL + 23 * 3600000 + 59 * 60000 + 59 * 1000 + 999);
}

TEST_CASE("parse_date_ms: invalid input rejected") {
    std::uint64_t ms = 0;
    CHECK_FALSE(parse_date_ms("nonsense", false, ms));
    CHECK_FALSE(parse_date_ms("2021-13-01", false, ms)); // month
    CHECK_FALSE(parse_date_ms("2021-01-32", false, ms)); // day
    CHECK_FALSE(parse_date_ms("2021-00-10", false, ms)); // month 0
    CHECK_FALSE(parse_date_ms("2021/01/10", false, ms)); // wrong separator
}

TEST_CASE("parse_date_ms: from < to within a day, monotonic under snowflake") {
    std::uint64_t f = 0, t = 0;
    REQUIRE(parse_date_ms("2022-06-15", false, f));
    REQUIRE(parse_date_ms("2022-06-15", true, t));
    CHECK(f < t);
    CHECK(ms_to_snowflake(f) < ms_to_snowflake(t));
}

TEST_CASE("human_duration: formatting buckets") {
    CHECK(human_duration(5) == "~5s");
    CHECK(human_duration(59) == "~59s");
    CHECK(human_duration(90) == "~1m");
    CHECK(human_duration(3600) == "~1h 0m");
    CHECK(human_duration(3660) == "~1h 1m");
    CHECK(human_duration(7325) == "~2h 2m");
}

TEST_CASE("format_uptime: omits leading zero units") {
    CHECK(format_uptime(0) == "0s");
    CHECK(format_uptime(45) == "45s");
    CHECK(format_uptime(90) == "1m 30s");
    CHECK(format_uptime(3600) == "1h 0m 0s");
    CHECK(format_uptime(90061) == "1d 1h 1m 1s");
    CHECK(format_uptime(-5) == "0s"); // clamps negatives
}
