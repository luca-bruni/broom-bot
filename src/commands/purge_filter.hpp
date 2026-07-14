#pragma once

#include <cctype>
#include <cstdint>
#include <regex>
#include <string>
#include <vector>

namespace broom::commands {

// Pure, DPP-free view of the fields a purge filter inspects. The scan job
// builds one from each dpp::message so the matching logic can be unit-tested
// without constructing live Discord objects.
struct MessageView {
    std::string content;
    std::uint64_t author_id = 0;
    bool is_bot = false;
    bool has_attachment = false;
    bool has_embed = false;
};

struct PurgeParams {
    std::string scope; // "channel" | "guild"
    std::vector<std::string> keywords;
    std::string pattern;         // regex (ECMAScript, case-insensitive); empty = none
    std::uint64_t author_id = 0; // only this author; 0 = any
    std::string has;             // "", "attachment", "link", or "embed"
    bool bots_only = false;
    std::uint64_t channel_id = 0; // channel scope target; 0 for guild
    std::uint64_t from_id = 0;    // lower snowflake bound (0 = none)
    std::uint64_t to_id = 0;      // upper snowflake bound (0 = latest)
};

inline std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::vector<std::string> parse_keywords(const std::string& raw) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        auto comma = raw.find(',', start);
        auto end = comma == std::string::npos ? raw.size() : comma;
        std::string token = raw.substr(start, end - start);
        auto first = token.find_first_not_of(" \t");
        auto last = token.find_last_not_of(" \t");
        if (first != std::string::npos)
            out.push_back(to_lower(token.substr(first, last - first + 1)));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

inline bool keyword_match(const std::string& content,
                          const std::vector<std::string>& keywords) {
    std::string lc = to_lower(content);
    for (const auto& k : keywords) {
        if (!k.empty() && lc.find(k) != std::string::npos) return true;
    }
    return false;
}

inline bool has_link(const std::string& content) {
    return content.find("http://") != std::string::npos ||
           content.find("https://") != std::string::npos;
}

// A message matches when it passes every provided filter. The keyword/pattern
// content filter is an OR of the two; other filters are AND-combined. `re` is
// the compiled pattern (nullptr when no pattern was given). Snowflake range
// bounds (from_id/to_id) are enforced by pagination, not here.
inline bool message_matches(const MessageView& m, const PurgeParams& p, const std::regex* re) {
    if (p.author_id && m.author_id != p.author_id) return false;
    if (p.bots_only && !m.is_bot) return false;
    if (p.has == "attachment" && !m.has_attachment) return false;
    if (p.has == "link" && !has_link(m.content)) return false;
    if (p.has == "embed" && !m.has_embed) return false;

    const bool have_content_filter = !p.keywords.empty() || re != nullptr;
    if (have_content_filter) {
        bool ok = keyword_match(m.content, p.keywords);
        if (!ok && re) {
            try {
                ok = std::regex_search(m.content, *re);
            } catch (const std::regex_error&) {
                ok = false;
            }
        }
        if (!ok) return false;
    }
    return true;
}

} // namespace broom::commands
