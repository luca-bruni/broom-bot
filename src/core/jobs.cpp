#include "core/jobs.hpp"

#include <charconv>
#include <chrono>
#include <system_error>

namespace broom {

namespace {

std::int64_t now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

dpp::message progress_message_skeleton(std::int64_t job_id, const std::string& text,
                                       bool with_cancel) {
    dpp::message msg(text);
    if (with_cancel) {
        msg.add_component(dpp::component().add_component(
            dpp::component()
                .set_type(dpp::cot_button)
                .set_label("Cancel")
                .set_style(dpp::cos_danger)
                .set_id("job:cancel:" + std::to_string(job_id))));
    }
    return msg;
}

} // namespace

const std::vector<std::string>& job_schema() {
    static const std::vector<std::string> steps = {
        R"sql(
        CREATE TABLE jobs(
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            kind TEXT NOT NULL,
            params TEXT NOT NULL,
            status TEXT NOT NULL DEFAULT 'queued',
            started_by INTEGER NOT NULL,
            progress_channel INTEGER,
            progress_message INTEGER,
            scanned INTEGER NOT NULL DEFAULT 0,
            matched INTEGER NOT NULL DEFAULT 0,
            actioned INTEGER NOT NULL DEFAULT 0,
            note TEXT,
            created_at INTEGER NOT NULL,
            finished_at INTEGER
        );
        CREATE INDEX idx_jobs_guild_status ON jobs(guild_id, status);
        CREATE TABLE job_cursors(
            job_id INTEGER NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
            channel_id INTEGER NOT NULL,
            cursor_id INTEGER NOT NULL,
            PRIMARY KEY(job_id, channel_id)
        );
        CREATE TABLE guild_settings(
            guild_id INTEGER PRIMARY KEY,
            ingest_mode TEXT NOT NULL DEFAULT 'manual'
        );
        )sql",
    };
    return steps;
}

// --- JobContext -------------------------------------------------------------

JobContext::JobContext(JobRunner& runner, dpp::cluster& bot_ref, Db& db_ref,
                       std::int64_t id, dpp::snowflake guild, std::string params_str)
    : bot(bot_ref), db(db_ref), job_id(id), guild_id(guild),
      params(std::move(params_str)), runner_(runner) {}

bool JobContext::cancelled() const {
    return runner_.job_cancelled(job_id);
}

void JobContext::progress(std::int64_t scanned, std::int64_t matched,
                          std::int64_t actioned, const std::string& phase) {
    db.prepare("UPDATE jobs SET scanned=?1, matched=?2, actioned=?3 WHERE id=?4")
        .bind(1, scanned)
        .bind(2, matched)
        .bind(3, actioned)
        .bind(4, job_id)
        .step();

    auto now = std::chrono::steady_clock::now();
    if (now - last_edit_ < std::chrono::seconds(3)) return;
    last_edit_ = now;

    runner_.edit_progress_message(
        job_id,
        "⏳ Job #" + std::to_string(job_id) + " — " + phase + "\n" +
            "Scanned: " + std::to_string(scanned) +
            " | Matched: " + std::to_string(matched) +
            " | Actioned: " + std::to_string(actioned),
        false);
}

void JobContext::save_cursor(dpp::snowflake channel_id, std::uint64_t cursor_id) {
    db.prepare("INSERT INTO job_cursors(job_id, channel_id, cursor_id) VALUES(?1,?2,?3) "
               "ON CONFLICT(job_id, channel_id) DO UPDATE SET cursor_id=excluded.cursor_id")
        .bind(1, job_id)
        .bind(2, static_cast<std::int64_t>(static_cast<std::uint64_t>(channel_id)))
        .bind(3, static_cast<std::int64_t>(cursor_id))
        .step();
}

std::optional<std::uint64_t> JobContext::load_cursor(dpp::snowflake channel_id) {
    auto stmt =
        db.prepare("SELECT cursor_id FROM job_cursors WHERE job_id=?1 AND channel_id=?2");
    stmt.bind(1, job_id)
        .bind(2, static_cast<std::int64_t>(static_cast<std::uint64_t>(channel_id)));
    if (!stmt.step()) return std::nullopt;
    return static_cast<std::uint64_t>(stmt.column_int(0));
}

// --- JobRunner --------------------------------------------------------------

JobRunner::JobRunner(dpp::cluster& bot, Db& db) : bot_(bot), db_(db) {}

JobRunner::~JobRunner() {
    stopping_ = true;
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void JobRunner::register_kind(const std::string& kind, JobFn fn) {
    kinds_[kind] = std::move(fn);
}

std::optional<std::int64_t> JobRunner::enqueue(dpp::snowflake guild_id,
                                               const std::string& kind,
                                               const std::string& params,
                                               dpp::snowflake progress_channel,
                                               dpp::snowflake started_by) {
    std::lock_guard lock(mutex_);

    auto busy = db_.prepare("SELECT COUNT(*) FROM jobs WHERE guild_id=?1 AND "
                            "status IN ('queued','running')");
    busy.bind(1, static_cast<std::int64_t>(static_cast<std::uint64_t>(guild_id)));
    busy.step();
    if (busy.column_int(0) > 0) return std::nullopt;

    db_.prepare("INSERT INTO jobs(guild_id, kind, params, started_by, progress_channel, "
                "created_at) VALUES(?1,?2,?3,?4,?5,?6)")
        .bind(1, static_cast<std::int64_t>(static_cast<std::uint64_t>(guild_id)))
        .bind(2, kind)
        .bind(3, params)
        .bind(4, static_cast<std::int64_t>(static_cast<std::uint64_t>(started_by)))
        .bind(5, static_cast<std::int64_t>(static_cast<std::uint64_t>(progress_channel)))
        .bind(6, now_seconds())
        .step();
    std::int64_t job_id = db_.last_insert_id();

    bot_.message_create(
        progress_message_skeleton(job_id, "⏳ Job #" + std::to_string(job_id) + " queued…",
                                  true)
            .set_channel_id(progress_channel),
        [this, job_id](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) return;
            auto message_id = std::get<dpp::message>(cb.value).id;
            db_.prepare("UPDATE jobs SET progress_message=?1 WHERE id=?2")
                .bind(1, static_cast<std::int64_t>(static_cast<std::uint64_t>(message_id)))
                .bind(2, job_id)
                .step();
        });

    wake_.notify_all();
    return job_id;
}

void JobRunner::start() {
    // Crash recovery: anything still 'running' was interrupted mid-flight.
    db_.exec("UPDATE jobs SET status='queued' WHERE status='running'");

    bot_.on_button_click([this](const dpp::button_click_t& event) {
        constexpr std::string_view prefix = "job:cancel:";
        if (event.custom_id.rfind(prefix, 0) != 0) return;

        // Parsed defensively — custom_ids arrive from clients.
        std::int64_t job_id = 0;
        const char* id_begin = event.custom_id.data() + prefix.size();
        const char* id_end = event.custom_id.data() + event.custom_id.size();
        auto [ptr, ec] = std::from_chars(id_begin, id_end, job_id);
        if (ec != std::errc{} || ptr != id_end) return;

        auto stmt = db_.prepare("SELECT started_by FROM jobs WHERE id=?1 AND "
                                "status IN ('queued','running')");
        stmt.bind(1, job_id);
        if (!stmt.step()) {
            event.reply(dpp::message("That job is no longer active.")
                            .set_flags(dpp::m_ephemeral));
            return;
        }
        if (static_cast<std::uint64_t>(stmt.column_int(0)) !=
            static_cast<std::uint64_t>(event.command.usr.id)) {
            event.reply(dpp::message("Only the user who started this job can cancel it.")
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        request_cancel(job_id);
        event.reply(dpp::message("Cancelling job #" + std::to_string(job_id) + "…")
                        .set_flags(dpp::m_ephemeral));
    });

    worker_ = std::thread([this] { worker_loop(); });
}

void JobRunner::worker_loop() {
    while (!stopping_) {
        std::int64_t next = 0;
        {
            std::unique_lock lock(mutex_);
            auto stmt =
                db_.prepare("SELECT id FROM jobs WHERE status='queued' ORDER BY id LIMIT 1");
            if (stmt.step()) {
                next = stmt.column_int(0);
            } else {
                wake_.wait_for(lock, std::chrono::seconds(2));
                continue;
            }
        }
        run_one(next);
    }
}

void JobRunner::run_one(std::int64_t job_id) {
    std::string kind, params;
    dpp::snowflake guild_id;
    {
        auto stmt = db_.prepare("SELECT kind, params, guild_id FROM jobs WHERE id=?1");
        stmt.bind(1, job_id);
        if (!stmt.step()) return;
        kind = stmt.column_text(0);
        params = stmt.column_text(1);
        guild_id = static_cast<std::uint64_t>(stmt.column_int(2));
    }

    auto it = kinds_.find(kind);
    if (it == kinds_.end()) {
        finish(job_id, "failed", "unknown job kind '" + kind + "'");
        return;
    }

    db_.prepare("UPDATE jobs SET status='running' WHERE id=?1").bind(1, job_id).step();

    JobContext ctx(*this, bot_, db_, job_id, guild_id, params);
    try {
        it->second(ctx);
        finish(job_id, ctx.cancelled() ? "cancelled" : "done", "");
    } catch (const std::exception& e) {
        finish(job_id, "failed", e.what());
        bot_.log(dpp::ll_error,
                 "Job #" + std::to_string(job_id) + " failed: " + e.what());
    }

    {
        std::lock_guard lock(mutex_);
        cancel_set_.erase(job_id);
    }
}

bool JobRunner::request_cancel(std::int64_t job_id) {
    std::lock_guard lock(mutex_);
    auto stmt = db_.prepare("SELECT status FROM jobs WHERE id=?1");
    stmt.bind(1, job_id);
    if (!stmt.step()) return false;
    std::string status = stmt.column_text(0);
    if (status != "queued" && status != "running") return false;
    cancel_set_.insert(job_id);
    wake_.notify_all();
    return true;
}

bool JobRunner::job_cancelled(std::int64_t job_id) {
    if (stopping_.load()) return true;
    std::lock_guard lock(mutex_);
    return cancel_set_.count(job_id) > 0;
}

void JobRunner::finish(std::int64_t job_id, const std::string& status,
                       const std::string& note) {
    db_.prepare("UPDATE jobs SET status=?1, note=?2, finished_at=?3 WHERE id=?4")
        .bind(1, status)
        .bind(2, note)
        .bind(3, now_seconds())
        .bind(4, job_id)
        .step();

    // The progress message is created asynchronously; a fast job can reach here
    // before its id is stored, which would skip the final edit and leave the
    // Cancel button on-screen. Wait briefly for the id to land.
    for (int i = 0; i < 20; ++i) {
        auto stmt = db_.prepare("SELECT progress_message FROM jobs WHERE id=?1");
        stmt.bind(1, job_id);
        if (stmt.step() && !stmt.column_is_null(0)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::int64_t scanned = 0, matched = 0, actioned = 0;
    {
        auto stmt =
            db_.prepare("SELECT scanned, matched, actioned FROM jobs WHERE id=?1");
        stmt.bind(1, job_id);
        if (stmt.step()) {
            scanned = stmt.column_int(0);
            matched = stmt.column_int(1);
            actioned = stmt.column_int(2);
        }
    }

    std::string emoji = status == "done" ? "✅" : status == "cancelled" ? "🛑" : "❌";
    std::string summary =
        emoji + " Job #" + std::to_string(job_id) + " " + status +
        (note.empty() ? "" : " — " + note) + "\nScanned: " + std::to_string(scanned) +
        " | Matched: " + std::to_string(matched) +
        " | Actioned: " + std::to_string(actioned);

    // Update the existing progress message in place (no duplicate post).
    edit_progress_message(job_id, summary, true);
}

void JobRunner::edit_progress_message(std::int64_t job_id, const std::string& text,
                                      bool remove_button) {
    auto stmt = db_.prepare(
        "SELECT progress_channel, progress_message FROM jobs WHERE id=?1");
    stmt.bind(1, job_id);
    if (!stmt.step() || stmt.column_is_null(1)) return;

    dpp::message msg = progress_message_skeleton(job_id, text, !remove_button);
    msg.id = static_cast<std::uint64_t>(stmt.column_int(1));
    msg.channel_id = static_cast<std::uint64_t>(stmt.column_int(0));
    bot_.message_edit(msg);
}

} // namespace broom
