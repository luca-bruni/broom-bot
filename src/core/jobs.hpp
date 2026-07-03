#pragma once

#include "core/db.hpp"

#include <dpp/dpp.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace broom {

class JobRunner;

// Handed to a job function while it runs. All methods are safe to call from
// the worker thread; REST calls go through DPP's thread-safe queues.
class JobContext {
public:
    dpp::cluster& bot;
    Db& db;
    const std::int64_t job_id;
    const dpp::snowflake guild_id;
    const std::string params; // job-kind-defined (e.g. JSON); opaque to the runner

    bool cancelled() const;

    // Updates counters and (throttled, ~3s) edits the progress message.
    void progress(std::int64_t scanned, std::int64_t matched, std::int64_t actioned,
                  const std::string& phase);

    // Per-channel resume checkpoint.
    void save_cursor(dpp::snowflake channel_id, std::uint64_t cursor_id);
    std::optional<std::uint64_t> load_cursor(dpp::snowflake channel_id);

private:
    friend class JobRunner;
    JobContext(JobRunner& runner, dpp::cluster& bot, Db& db, std::int64_t job_id,
               dpp::snowflake guild_id, std::string params);
    JobRunner& runner_;
    std::chrono::steady_clock::time_point last_edit_{};
};

using JobFn = std::function<void(JobContext&)>;

// SQLite-backed background job runner. One worker thread, one job at a time,
// at most one active job per guild. Jobs found in 'running' state at startup
// (crash) are re-queued; job functions must therefore be resumable via cursors.
class JobRunner {
public:
    JobRunner(dpp::cluster& bot, Db& db);
    ~JobRunner();
    JobRunner(const JobRunner&) = delete;
    JobRunner& operator=(const JobRunner&) = delete;

    void register_kind(const std::string& kind, JobFn fn);

    // Rejects (returns nullopt) if the guild already has a queued/running job.
    // Posts a progress message (with Cancel button) to progress_channel.
    std::optional<std::int64_t> enqueue(dpp::snowflake guild_id, const std::string& kind,
                                        const std::string& params,
                                        dpp::snowflake progress_channel,
                                        dpp::snowflake started_by);

    // Wires the Cancel button ("job:cancel:<id>", allowed for the job starter),
    // re-queues crashed jobs, and starts the worker. Call once after handlers
    // are registered.
    void start();

private:
    friend class JobContext;

    void worker_loop();
    void run_one(std::int64_t job_id);
    void finish(std::int64_t job_id, const std::string& status, const std::string& note);
    void edit_progress_message(std::int64_t job_id, const std::string& text,
                               bool remove_button);

    dpp::cluster& bot_;
    Db& db_;
    std::map<std::string, JobFn> kinds_;

    std::mutex mutex_;
    std::condition_variable wake_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::int64_t> cancel_requested_{0};
    std::int64_t active_job_{0};
    std::thread worker_;
};

// Schema steps for the job system; passed to Db::migrate() at startup.
const std::vector<std::string>& job_schema();

} // namespace broom
