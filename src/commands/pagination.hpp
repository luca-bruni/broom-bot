#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace broom::commands {

// Pure pagination logic for list commands, kept DPP-free so it can be
// unit-tested. The matching prev/next button row lives in embeds.hpp
// (page_buttons) and follows the "<command>:page:<n>" custom_id convention.

inline constexpr int kPageSize = 10;

struct PageView {
    std::string body; // the page's lines, newline-terminated
    int page = 0;     // 0-based, clamped
    int pages = 1;    // total page count (>= 1)
};

inline PageView paginate(const std::vector<std::string>& lines, int page,
                         int per_page = kPageSize) {
    PageView view;
    view.pages = std::max<int>(1, static_cast<int>((lines.size() + per_page - 1) /
                                                   static_cast<std::size_t>(per_page)));
    view.page = std::clamp(page, 0, view.pages - 1);
    std::size_t begin = static_cast<std::size_t>(view.page) * per_page;
    std::size_t end = std::min(begin + per_page, lines.size());
    for (std::size_t i = begin; i < end; ++i) view.body += lines[i] + "\n";
    return view;
}

// For a custom_id "<command>:page:<n>", returns n; nullopt otherwise.
inline std::optional<int> parse_page_custom_id(const std::string& custom_id) {
    auto marker = custom_id.find(":page:");
    if (marker == std::string::npos) return std::nullopt;
    const char* begin = custom_id.data() + marker + 6;
    const char* end = custom_id.data() + custom_id.size();
    int page = 0;
    auto [ptr, ec] = std::from_chars(begin, end, page);
    if (ec != std::errc{} || ptr != end) return std::nullopt;
    return page;
}

} // namespace broom::commands
