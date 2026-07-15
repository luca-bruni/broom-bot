#pragma once

#include "core/db.hpp"

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace broom {

// Daily SQLite backups: VACUUM INTO <data_dir>/backups/broom-<UTC stamp>.db,
// keeping the newest kKeep files. Runs once at startup, then every 24h, on
// its own thread (VACUUM INTO is safe against a live WAL database; the Db is
// opened serialized). Owned by main; RAII shutdown like the other services.
class BackupService {
public:
    static constexpr int kKeep = 7;

    BackupService(Db& db, std::string data_dir);
    ~BackupService();
    BackupService(const BackupService&) = delete;
    BackupService& operator=(const BackupService&) = delete;

    void start();

    // One backup + prune, immediately. Returns the created file's path.
    // Throws on failure (worker loop catches and logs).
    std::string backup_now();

private:
    void loop();
    void prune();

    Db& db_;
    std::string backup_dir_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
    std::thread worker_;
};

} // namespace broom
