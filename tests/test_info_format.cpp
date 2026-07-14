#include "doctest.h"

#include "commands/eightball_answers.hpp"
#include "commands/info_format.hpp"

using namespace broom::commands;

TEST_CASE("format_color: uppercase 6-digit hex") {
    CHECK(format_color(0x000000) == "#000000");
    CHECK(format_color(0xFF0000) == "#FF0000");
    CHECK(format_color(0x5865F2) == "#5865F2");
    CHECK(format_color(0xFFFFFF) == "#FFFFFF");
    CHECK(format_color(0x1) == "#000001");
}

TEST_CASE("channel_type_name: known and unknown") {
    CHECK(channel_type_name(0) == "Text");
    CHECK(channel_type_name(2) == "Voice");
    CHECK(channel_type_name(4) == "Category");
    CHECK(channel_type_name(5) == "Announcement");
    CHECK(channel_type_name(15) == "Forum");
    CHECK(channel_type_name(999) == "Unknown");
}

TEST_CASE("parse_custom_emoji: valid static and animated") {
    auto s = parse_custom_emoji("<:smile:123456789012345678>");
    REQUIRE(s.has_value());
    CHECK_FALSE(s->animated);
    CHECK(s->name == "smile");
    CHECK(s->id == 123456789012345678ULL);

    auto a = parse_custom_emoji("<a:party:987654321>");
    REQUIRE(a.has_value());
    CHECK(a->animated);
    CHECK(a->name == "party");
    CHECK(a->id == 987654321ULL);
}

TEST_CASE("parse_custom_emoji: whitespace tolerated") {
    auto s = parse_custom_emoji("  <:ok:42>  ");
    REQUIRE(s.has_value());
    CHECK(s->id == 42);
}

TEST_CASE("parse_custom_emoji: invalid rejected") {
    CHECK_FALSE(parse_custom_emoji("😀").has_value());
    CHECK_FALSE(parse_custom_emoji("smile").has_value());
    CHECK_FALSE(parse_custom_emoji("<:smile:>").has_value()); // no id
    CHECK_FALSE(parse_custom_emoji("<::123>").has_value());   // no name
    CHECK_FALSE(parse_custom_emoji("<:sm:abc>").has_value()); // non-numeric id
    CHECK_FALSE(parse_custom_emoji("<:x:1>").has_value());    // name too short
    CHECK_FALSE(parse_custom_emoji("").has_value());
}

TEST_CASE("emoji_url: png vs gif") {
    CHECK(emoji_url(42, false) == "https://cdn.discordapp.com/emojis/42.png?size=256");
    CHECK(emoji_url(42, true) == "https://cdn.discordapp.com/emojis/42.gif?size=256");
}

TEST_CASE("eightball_answers: usable pool") {
    const auto& a = eightball_answers();
    CHECK(a.size() >= 10);
    for (const auto& answer : a) CHECK_FALSE(answer.empty());
}
