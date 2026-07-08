#include "doctest.h"

#include "commands/purge_filter.hpp"

#include <regex>

using namespace broom::commands;

namespace {
MessageView mv(std::string content, std::uint64_t author = 0, bool bot = false,
               bool attachment = false, bool embed = false) {
    MessageView m;
    m.content = std::move(content);
    m.author_id = author;
    m.is_bot = bot;
    m.has_attachment = attachment;
    m.has_embed = embed;
    return m;
}
} // namespace

TEST_CASE("parse_keywords: split, trim, lowercase") {
    auto k = parse_keywords("foo, Bar ,  baz");
    REQUIRE(k.size() == 3);
    CHECK(k[0] == "foo");
    CHECK(k[1] == "bar");
    CHECK(k[2] == "baz");

    CHECK(parse_keywords("").empty());
    CHECK(parse_keywords("  ,  , ").empty());

    auto one = parse_keywords("Single");
    REQUIRE(one.size() == 1);
    CHECK(one[0] == "single");
}

TEST_CASE("keyword_match: case-insensitive substring") {
    std::vector<std::string> kws{"spam", "scam"};
    CHECK(keyword_match("This is SPAM", kws));
    CHECK(keyword_match("a scammy thing", kws));
    CHECK_FALSE(keyword_match("clean text", kws));
    CHECK_FALSE(keyword_match("anything", {}));
}

TEST_CASE("has_link") {
    CHECK(has_link("go to http://x.com"));
    CHECK(has_link("secure https://y.com"));
    CHECK_FALSE(has_link("no url here"));
}

TEST_CASE("message_matches: no content filter matches everything") {
    PurgeParams p;
    CHECK(message_matches(mv("anything"), p, nullptr));
}

TEST_CASE("message_matches: keywords") {
    PurgeParams p;
    p.keywords = {"spam"};
    CHECK(message_matches(mv("SPAM here"), p, nullptr));
    CHECK_FALSE(message_matches(mv("clean"), p, nullptr));
}

TEST_CASE("message_matches: author filter is AND") {
    PurgeParams p;
    p.author_id = 42;
    p.keywords = {"x"};
    CHECK(message_matches(mv("x", 42), p, nullptr));
    CHECK_FALSE(message_matches(mv("x", 99), p, nullptr)); // wrong author
    CHECK_FALSE(message_matches(mv("y", 42), p, nullptr)); // right author, no content match
}

TEST_CASE("message_matches: bots_only") {
    PurgeParams p;
    p.bots_only = true;
    CHECK(message_matches(mv("hi", 1, true), p, nullptr));
    CHECK_FALSE(message_matches(mv("hi", 1, false), p, nullptr));
}

TEST_CASE("message_matches: has attachment/link/embed") {
    PurgeParams att;
    att.has = "attachment";
    CHECK(message_matches(mv("f", 0, false, true, false), att, nullptr));
    CHECK_FALSE(message_matches(mv("f"), att, nullptr));

    PurgeParams link;
    link.has = "link";
    CHECK(message_matches(mv("see http://z.com"), link, nullptr));
    CHECK_FALSE(message_matches(mv("no link"), link, nullptr));

    PurgeParams emb;
    emb.has = "embed";
    CHECK(message_matches(mv("f", 0, false, false, true), emb, nullptr));
    CHECK_FALSE(message_matches(mv("f"), emb, nullptr));
}

TEST_CASE("message_matches: regex pattern") {
    PurgeParams p;
    p.pattern = "^!";
    std::regex re(p.pattern, std::regex::ECMAScript | std::regex::icase);
    CHECK(message_matches(mv("!command"), p, &re));
    CHECK_FALSE(message_matches(mv("normal"), p, &re));
}

TEST_CASE("message_matches: keywords OR pattern") {
    PurgeParams p;
    p.keywords = {"spam"};
    p.pattern = "^!";
    std::regex re(p.pattern, std::regex::ECMAScript | std::regex::icase);
    CHECK(message_matches(mv("this is spam"), p, &re)); // keyword
    CHECK(message_matches(mv("!cmd"), p, &re));         // pattern
    CHECK_FALSE(message_matches(mv("neither"), p, &re));
}

TEST_CASE("message_matches: combined author + has + content") {
    PurgeParams p;
    p.author_id = 7;
    p.has = "link";
    p.keywords = {"buy"};
    CHECK(message_matches(mv("buy now http://x", 7), p, nullptr));
    CHECK_FALSE(message_matches(mv("buy now http://x", 8), p, nullptr)); // wrong author
    CHECK_FALSE(message_matches(mv("buy now", 7), p, nullptr));          // no link
    CHECK_FALSE(message_matches(mv("hello http://x", 7), p, nullptr));   // no keyword
}
