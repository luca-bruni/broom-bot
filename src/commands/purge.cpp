#include "commands/purge.hpp"

#include "core/db.hpp"
#include "core/jobs.hpp"

#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#define timegm _mkgmtime
#endif

namespace broom::commands {

namespace {

constexpr std::uint64_t kDiscordEpochMs = 1420070400000ULL;
constexpr std::int64_t kBulkDeletableMs = 14LL * 24 * 60 * 60 * 1000; // <14 days
// Cursor sentinel meaning "channel fully scanned"; a real message id is never 1.
constexpr std::uint64_t kChannelDone = 1;

std::uint64_t ms_to_snowflake(std::uint64_t ms) {
    return ms <= kDiscordEpochMs ? 0 : ((ms - kDiscordEpochMs) << 22);
}

// Parses YYYY-MM-DD as UTC. end_of_day pushes to 23:59:59.999.
bool parse_date_ms(const std::string& s, bool end_of_day, std::uint64_t& out_ms) {
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

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> parse_keywords(const std::string& raw) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        auto comma = raw.find(',', start);
        auto end = comma == std::string::npos ? raw.size() : comma;
        std::string token = raw.substr(start, end - start);
        auto first = token.find_first_not_of(" \t");
        auto last = token.find_last_not_of(" \t");
        if (first != std::string::npos) out.push_back(to_lower(token.substr(first, last - first + 1)));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

bool content_matches(const std::string& content, const std::vector<std::string>& keywords) {
    std::string lc = to_lower(content);
    for (const auto& k : keywords) {
        if (!k.empty() && lc.find(k) != std::string::npos) return true;
    }
    return false;
}

std::string human_duration(std::int64_t seconds) {
    if (seconds < 60) return "~" + std::to_string(seconds) + "s";
    if (seconds < 3600) return "~" + std::to_string(seconds / 60) + "m";
    return "~" + std::to_string(seconds / 3600) + "h " + std::to_string((seconds % 3600) / 60) + "m";
}

struct PurgeParams {
    std::string scope; // "channel" | "guild"
    std::vector<std::string> keywords;
    std::uint64_t channel_id = 0; // channel scope target; 0 for guild
    std::uint64_t from_id = 0;     // lower snowflake bound (0 = none)
    std::uint64_t to_id = 0;       // upper snowflake bound (0 = latest)
};

std::string encode_params(const PurgeParams& p) {
    nlohmann::json j;
    j["scope"] = p.scope;
    j["keywords"] = p.keywords;
    j["channel_id"] = p.channel_id;
    j["from_id"] = p.from_id;
    j["to_id"] = p.to_id;
    return j.dump();
}

PurgeParams decode_params(const std::string& s) {
    auto j = nlohmann::json::parse(s);
    PurgeParams p;
    p.scope = j.at("scope").get<std::string>();
    p.keywords = j.at("keywords").get<std::vector<std::string>>();
    p.channel_id = j.value("channel_id", std::uint64_t{0});
    p.from_id = j.value("from_id", std::uint64_t{0});
    p.to_id = j.value("to_id", std::uint64_t{0});
    return p;
}

// --- Blocking REST wrappers (worker thread) --------------------------------
// A shared_ptr-owned promise keeps the callback safe even if we abandon the
// wait on cancellation.

struct Page {
    bool ok = false;
    bool cancelled = false;
    std::vector<dpp::message> messages;
};

Page fetch_page(JobContext& ctx, dpp::snowflake channel, std::uint64_t before) {
    auto prom = std::make_shared<std::promise<Page>>();
    auto fut = prom->get_future();
    ctx.bot.messages_get(
        channel, 0, dpp::snowflake(before), 0, 100,
        [prom](const dpp::confirmation_callback_t& cb) {
            Page p;
            if (!cb.is_error()) {
                p.ok = true;
                const auto& mm = std::get<dpp::message_map>(cb.value);
                p.messages.reserve(mm.size());
                for (const auto& [id, m] : mm) p.messages.push_back(m);
            }
            prom->set_value(std::move(p));
        });
    while (fut.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
        if (ctx.cancelled()) {
            Page p;
            p.cancelled = true;
            return p;
        }
    }
    return fut.get();
}

// Returns true on success (message gone, whether we deleted it or it was
// already gone). false only on a hard failure we should surface.
bool delete_one(JobContext& ctx, dpp::snowflake channel, dpp::snowflake message) {
    auto prom = std::make_shared<std::promise<bool>>();
    auto fut = prom->get_future();
    ctx.bot.message_delete(message, channel, [prom](const dpp::confirmation_callback_t& cb) {
        prom->set_value(!cb.is_error());
    });
    while (fut.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
        if (ctx.cancelled()) return false;
    }
    return fut.get();
}

bool delete_bulk(JobContext& ctx, dpp::snowflake channel,
                 const std::vector<dpp::snowflake>& ids) {
    auto prom = std::make_shared<std::promise<bool>>();
    auto fut = prom->get_future();
    ctx.bot.message_delete_bulk(ids, channel, [prom](const dpp::confirmation_callback_t& cb) {
        prom->set_value(!cb.is_error());
    });
    while (fut.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
        if (ctx.cancelled()) return false;
    }
    return fut.get();
}

std::vector<dpp::snowflake> resolve_guild_channels(JobContext& ctx) {
    std::vector<dpp::snowflake> channels;
    dpp::guild* guild = dpp::find_guild(ctx.guild_id);
    if (guild) {
        for (auto cid : guild->channels) {
            dpp::channel* ch = dpp::find_channel(cid);
            if (ch && (ch->is_text_channel() || ch->is_news_channel())) channels.push_back(cid);
        }
    }
    // Active threads (archived threads are out of scope for v1).
    auto prom = std::make_shared<std::promise<std::vector<dpp::snowflake>>>();
    auto fut = prom->get_future();
    ctx.bot.threads_get_active(ctx.guild_id, [prom](const dpp::confirmation_callback_t& cb) {
        std::vector<dpp::snowflake> t;
        if (!cb.is_error()) {
            const auto& at = std::get<dpp::active_threads>(cb.value);
            for (const auto& [id, info] : at) t.push_back(id);
        }
        prom->set_value(std::move(t));
    });
    while (fut.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
        if (ctx.cancelled()) return channels;
    }
    auto threads = fut.get();
    channels.insert(channels.end(), threads.begin(), threads.end());
    return channels;
}

std::uint64_t progress_channel_of(JobContext& ctx) {
    auto stmt = ctx.db.prepare("SELECT progress_channel FROM jobs WHERE id=?1");
    stmt.bind(1, ctx.job_id);
    if (stmt.step() && !stmt.column_is_null(0)) {
        return static_cast<std::uint64_t>(stmt.column_int(0));
    }
    return 0;
}

// --- Scan job ---------------------------------------------------------------

void run_purge_scan(JobContext& ctx) {
    PurgeParams p = decode_params(ctx.params);

    std::vector<dpp::snowflake> channels;
    if (p.scope == "channel") {
        channels.push_back(dpp::snowflake(p.channel_id));
    } else {
        channels = resolve_guild_channels(ctx);
    }

    std::int64_t scanned = 0, matched = 0, inaccessible = 0;

    for (auto channel : channels) {
        if (ctx.cancelled()) break;

        std::uint64_t before =
            ctx.load_cursor(channel).value_or(p.to_id ? p.to_id : 0);
        if (before == kChannelDone) continue; // finished on a previous run

        bool done = false;
        while (!done && !ctx.cancelled()) {
            Page page = fetch_page(ctx, channel, before);
            if (page.cancelled) break;
            if (!page.ok) {
                ++inaccessible; // missing Read Message History / Manage Messages
                break;
            }
            if (page.messages.empty()) {
                done = true;
                break;
            }
            std::sort(page.messages.begin(), page.messages.end(),
                      [](const dpp::message& a, const dpp::message& b) {
                          return static_cast<std::uint64_t>(a.id) >
                                 static_cast<std::uint64_t>(b.id);
                      });

            std::uint64_t oldest = before;
            for (const auto& m : page.messages) {
                auto mid = static_cast<std::uint64_t>(m.id);
                if (p.from_id && mid < p.from_id) {
                    done = true;
                    break;
                }
                ++scanned;
                oldest = mid;
                if (content_matches(m.content, p.keywords)) {
                    auto created_ms =
                        static_cast<std::int64_t>(m.id.get_creation_time() * 1000.0);
                    ctx.db
                        .prepare("INSERT OR IGNORE INTO purge_matches"
                                 "(scan_job_id, channel_id, message_id, created_at, deleted) "
                                 "VALUES(?1,?2,?3,?4,0)")
                        .bind(1, ctx.job_id)
                        .bind(2, static_cast<std::int64_t>(static_cast<std::uint64_t>(channel)))
                        .bind(3, static_cast<std::int64_t>(mid))
                        .bind(4, created_ms)
                        .step();
                    ++matched;
                }
            }
            before = oldest;
            ctx.save_cursor(channel, before);
            ctx.progress(scanned, matched, 0, "scanning");
            if (page.messages.size() < 100) done = true;
        }
        if (done) ctx.save_cursor(channel, kChannelDone);
    }

    if (ctx.cancelled()) return; // runner records 'cancelled'

    std::uint64_t pchan = progress_channel_of(ctx);
    if (pchan == 0) return;

    if (matched == 0) {
        std::string note = inaccessible > 0
                               ? " (" + std::to_string(inaccessible) +
                                     " channel(s) skipped — missing permissions)"
                               : "";
        ctx.bot.message_create(dpp::message(
            dpp::snowflake(pchan), "🧹 No messages matched — nothing to delete." + note));
        return;
    }

    // Rough estimate: ~1s per bulk batch of 100 recent, ~1s per old message.
    std::int64_t recent = 0, old = 0;
    {
        auto stmt = ctx.db.prepare(
            "SELECT SUM(created_at >= ?1), SUM(created_at < ?1) FROM purge_matches "
            "WHERE scan_job_id=?2");
        stmt.bind(1, now_ms() - kBulkDeletableMs).bind(2, ctx.job_id);
        stmt.step();
        recent = stmt.column_int(0);
        old = stmt.column_int(1);
    }
    std::int64_t est = static_cast<std::int64_t>(std::ceil(recent / 100.0)) + old;

    std::string range = (p.from_id || p.to_id) ? " in the given date range" : "";
    std::string skipped = inaccessible > 0
                              ? "\n⚠️ " + std::to_string(inaccessible) +
                                    " channel(s) skipped (missing permissions)."
                              : "";

    dpp::message prompt(
        dpp::snowflake(pchan),
        "🧹 Found **" + std::to_string(matched) + "** matching message(s)" + range +
            ".\nEstimated deletion time: **" + human_duration(est) + "**." + skipped +
            "\n\n**Delete them?** This cannot be undone.");
    prompt.add_component(dpp::component()
                             .add_component(dpp::component()
                                                .set_type(dpp::cot_button)
                                                .set_label("Confirm delete")
                                                .set_style(dpp::cos_danger)
                                                .set_id("purge:confirm:" + std::to_string(ctx.job_id)))
                             .add_component(dpp::component()
                                                .set_type(dpp::cot_button)
                                                .set_label("Cancel")
                                                .set_style(dpp::cos_secondary)
                                                .set_id("purge:cancel:" + std::to_string(ctx.job_id))));
    ctx.bot.message_create(prompt);
}

// --- Delete job -------------------------------------------------------------

void mark_deleted(JobContext& ctx, std::int64_t scan_job_id, std::uint64_t message_id) {
    ctx.db
        .prepare("UPDATE purge_matches SET deleted=1 WHERE scan_job_id=?1 AND message_id=?2")
        .bind(1, scan_job_id)
        .bind(2, static_cast<std::int64_t>(message_id))
        .step();
}

void run_purge_delete(JobContext& ctx) {
    std::int64_t scan_job_id = nlohmann::json::parse(ctx.params).at("scan_job_id").get<std::int64_t>();

    std::int64_t total = 0;
    {
        auto stmt = ctx.db.prepare(
            "SELECT COUNT(*) FROM purge_matches WHERE scan_job_id=?1 AND deleted=0");
        stmt.bind(1, scan_job_id);
        stmt.step();
        total = stmt.column_int(0);
    }

    std::int64_t threshold = now_ms() - kBulkDeletableMs;
    std::int64_t actioned = 0;

    // Distinct channels with pending matches.
    std::vector<std::uint64_t> channels;
    {
        auto stmt = ctx.db.prepare("SELECT DISTINCT channel_id FROM purge_matches "
                                   "WHERE scan_job_id=?1 AND deleted=0");
        stmt.bind(1, scan_job_id);
        while (stmt.step()) channels.push_back(static_cast<std::uint64_t>(stmt.column_int(0)));
    }

    for (auto ch64 : channels) {
        if (ctx.cancelled()) break;
        dpp::snowflake channel(ch64);

        // Recent messages: bulk delete in batches of 100.
        while (!ctx.cancelled()) {
            std::vector<dpp::snowflake> batch;
            {
                auto stmt = ctx.db.prepare(
                    "SELECT message_id FROM purge_matches WHERE scan_job_id=?1 AND "
                    "channel_id=?2 AND deleted=0 AND created_at >= ?3 "
                    "ORDER BY message_id DESC LIMIT 100");
                stmt.bind(1, scan_job_id)
                    .bind(2, static_cast<std::int64_t>(ch64))
                    .bind(3, threshold);
                while (stmt.step())
                    batch.push_back(dpp::snowflake(static_cast<std::uint64_t>(stmt.column_int(0))));
            }
            if (batch.empty()) break;

            if (batch.size() >= 2 && delete_bulk(ctx, channel, batch)) {
                for (auto id : batch) mark_deleted(ctx, scan_job_id, static_cast<std::uint64_t>(id));
                actioned += static_cast<std::int64_t>(batch.size());
            } else {
                // Single message, or bulk failed — fall back to per-message.
                for (auto id : batch) {
                    if (ctx.cancelled()) break;
                    if (delete_one(ctx, channel, id)) ++actioned;
                    mark_deleted(ctx, scan_job_id, static_cast<std::uint64_t>(id));
                }
            }
            ctx.progress(0, total, actioned, "deleting");
        }

        // Old messages (>14 days): one at a time.
        while (!ctx.cancelled()) {
            std::uint64_t message_id = 0;
            {
                auto stmt = ctx.db.prepare(
                    "SELECT message_id FROM purge_matches WHERE scan_job_id=?1 AND "
                    "channel_id=?2 AND deleted=0 AND created_at < ?3 LIMIT 1");
                stmt.bind(1, scan_job_id)
                    .bind(2, static_cast<std::int64_t>(ch64))
                    .bind(3, threshold);
                if (!stmt.step()) break;
                message_id = static_cast<std::uint64_t>(stmt.column_int(0));
            }
            if (delete_one(ctx, channel, dpp::snowflake(message_id))) ++actioned;
            mark_deleted(ctx, scan_job_id, message_id);
            ctx.progress(0, total, actioned, "deleting");
        }
    }
}

} // namespace

dpp::slashcommand Purge::definition(dpp::snowflake app_id) const {
    dpp::slashcommand sc(name(), "Bulk-delete messages matching keywords", app_id);
    sc.set_default_permissions(dpp::p_manage_messages);

    dpp::command_option channel(dpp::co_sub_command, "channel", "Purge one channel");
    channel.add_option(dpp::command_option(dpp::co_string, "keywords",
                                           "Comma-separated keywords to match", true));
    channel.add_option(dpp::command_option(dpp::co_channel, "target",
                                           "Channel to purge (default: current)", false));
    channel.add_option(dpp::command_option(dpp::co_string, "from",
                                           "Only messages on/after YYYY-MM-DD", false));
    channel.add_option(dpp::command_option(dpp::co_string, "to",
                                           "Only messages on/before YYYY-MM-DD", false));

    dpp::command_option guild(dpp::co_sub_command, "guild", "Purge the entire server");
    guild.add_option(dpp::command_option(dpp::co_string, "keywords",
                                         "Comma-separated keywords to match", true));
    guild.add_option(dpp::command_option(dpp::co_string, "from",
                                         "Only messages on/after YYYY-MM-DD", false));
    guild.add_option(dpp::command_option(dpp::co_string, "to",
                                         "Only messages on/before YYYY-MM-DD", false));

    sc.add_option(channel);
    sc.add_option(guild);
    return sc;
}

void Purge::handle(const dpp::slashcommand_t& event) const {
    if (!event.command.guild_id) {
        event.reply(dpp::message("This command can only be used in a server.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    const auto& interaction = event.command.get_command_interaction();
    if (interaction.options.empty()) return;
    const auto& sub = interaction.options[0];

    auto get_string = [&](const std::string& n) -> std::optional<std::string> {
        for (const auto& o : sub.options) {
            if (o.name == n && std::holds_alternative<std::string>(o.value)) {
                return std::get<std::string>(o.value);
            }
        }
        return std::nullopt;
    };
    auto get_channel = [&](const std::string& n) -> std::uint64_t {
        for (const auto& o : sub.options) {
            if (o.name == n && std::holds_alternative<dpp::snowflake>(o.value)) {
                return static_cast<std::uint64_t>(std::get<dpp::snowflake>(o.value));
            }
        }
        return 0;
    };

    PurgeParams p;
    p.scope = sub.name;
    p.keywords = parse_keywords(get_string("keywords").value_or(""));
    if (p.keywords.empty()) {
        event.reply(dpp::message("Provide at least one keyword to match.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    if (auto from = get_string("from")) {
        std::uint64_t ms = 0;
        if (!parse_date_ms(*from, false, ms)) {
            event.reply(dpp::message("Invalid `from` date — use YYYY-MM-DD.")
                            .set_flags(dpp::m_ephemeral));
            return;
        }
        p.from_id = ms_to_snowflake(ms);
    }
    if (auto to = get_string("to")) {
        std::uint64_t ms = 0;
        if (!parse_date_ms(*to, true, ms)) {
            event.reply(dpp::message("Invalid `to` date — use YYYY-MM-DD.")
                            .set_flags(dpp::m_ephemeral));
            return;
        }
        p.to_id = ms_to_snowflake(ms);
    }

    if (p.scope == "channel") {
        std::uint64_t target = get_channel("target");
        p.channel_id = target ? target : static_cast<std::uint64_t>(event.command.channel_id);
    }

    auto job_id = services_->jobs.enqueue(event.command.guild_id, "purge_scan",
                                          encode_params(p), event.command.channel_id,
                                          event.command.usr.id);
    if (!job_id) {
        event.reply(dpp::message("A bulk job is already running for this server — "
                                 "wait for it to finish.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    event.reply(dpp::message("🧹 Scanning for matches — progress will post in this channel. "
                             "You'll be asked to confirm before anything is deleted.")
                    .set_flags(dpp::m_ephemeral));
}

void Purge::handle_button(const dpp::button_click_t& event) const {
    // custom_id: "purge:confirm:<scan_job_id>" or "purge:cancel:<scan_job_id>"
    auto first = event.custom_id.find(':');
    auto second = event.custom_id.find(':', first + 1);
    if (first == std::string::npos || second == std::string::npos) return;
    std::string action = event.custom_id.substr(first + 1, second - first - 1);
    std::int64_t scan_job_id = std::stoll(event.custom_id.substr(second + 1));

    std::int64_t started_by = 0;
    {
        auto stmt = services_->db.prepare("SELECT started_by FROM jobs WHERE id=?1");
        stmt.bind(1, scan_job_id);
        if (!stmt.step()) {
            event.reply(dpp::message("This purge is no longer available.")
                            .set_flags(dpp::m_ephemeral));
            return;
        }
        started_by = stmt.column_int(0);
    }
    if (static_cast<std::uint64_t>(started_by) !=
        static_cast<std::uint64_t>(event.command.usr.id)) {
        event.reply(dpp::message("Only the person who started this purge can confirm it.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    if (action == "cancel") {
        services_->db.prepare("DELETE FROM purge_matches WHERE scan_job_id=?1")
            .bind(1, scan_job_id)
            .step();
        event.reply(dpp::ir_update_message,
                    dpp::message("🚫 Purge cancelled — nothing was deleted."));
        return;
    }

    nlohmann::json params;
    params["scan_job_id"] = scan_job_id;
    auto del_id = services_->jobs.enqueue(event.command.guild_id, "purge_delete", params.dump(),
                                          event.command.channel_id, event.command.usr.id);
    if (!del_id) {
        event.reply(dpp::message("Another job is running for this server — try again shortly.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }
    event.reply(dpp::ir_update_message,
                dpp::message("🗑️ Deleting matched messages — progress will post below."));
}

void register_purge_jobs(JobRunner& jobs, Db&) {
    jobs.register_kind("purge_scan", run_purge_scan);
    jobs.register_kind("purge_delete", run_purge_delete);
}

const std::vector<std::string>& purge_schema() {
    static const std::vector<std::string> steps = {
        R"sql(
        CREATE TABLE purge_matches(
            scan_job_id INTEGER NOT NULL,
            channel_id  INTEGER NOT NULL,
            message_id  INTEGER NOT NULL,
            created_at  INTEGER NOT NULL,
            deleted     INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY(scan_job_id, message_id)
        );
        CREATE INDEX idx_purge_pending ON purge_matches(scan_job_id, channel_id, deleted);
        )sql",
    };
    return steps;
}

} // namespace broom::commands
