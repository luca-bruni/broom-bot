#include "doctest.h"

#include "commands/pagination.hpp"

#include <string>
#include <vector>

using namespace broom::commands;

static std::vector<std::string> lines(int n) {
    std::vector<std::string> v;
    for (int i = 0; i < n; ++i) v.push_back("line" + std::to_string(i));
    return v;
}

TEST_CASE("paginate: empty list is one empty page") {
    auto view = paginate({}, 0);
    CHECK(view.body.empty());
    CHECK(view.page == 0);
    CHECK(view.pages == 1);
}

TEST_CASE("paginate: exact page boundaries") {
    CHECK(paginate(lines(10), 0).pages == 1);
    CHECK(paginate(lines(11), 0).pages == 2);
    CHECK(paginate(lines(20), 0).pages == 2);
    CHECK(paginate(lines(21), 0).pages == 3);
}

TEST_CASE("paginate: slices the right lines") {
    auto view = paginate(lines(25), 1);
    CHECK(view.page == 1);
    CHECK(view.pages == 3);
    CHECK(view.body.find("line10\n") == 0);
    CHECK(view.body.find("line19") != std::string::npos);
    CHECK(view.body.find("line9\n") == std::string::npos);
    CHECK(view.body.find("line20") == std::string::npos);
}

TEST_CASE("paginate: out-of-range page clamps") {
    CHECK(paginate(lines(25), -5).page == 0);
    CHECK(paginate(lines(25), 99).page == 2);
    CHECK(paginate(lines(25), 99).body.find("line20") == 0);
}

TEST_CASE("parse_page_custom_id: valid") {
    CHECK(parse_page_custom_id("jobs:page:0") == 0);
    CHECK(parse_page_custom_id("remind:page:12") == 12);
    CHECK(parse_page_custom_id("x:page:-1") == -1); // clamped later by paginate
}

TEST_CASE("parse_page_custom_id: invalid") {
    CHECK_FALSE(parse_page_custom_id("jobs:list").has_value());
    CHECK_FALSE(parse_page_custom_id("jobs:page:").has_value());
    CHECK_FALSE(parse_page_custom_id("jobs:page:abc").has_value());
    CHECK_FALSE(parse_page_custom_id("jobs:page:1x").has_value());
    CHECK_FALSE(parse_page_custom_id("").has_value());
}
