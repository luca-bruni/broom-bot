#include "core/backup.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace broom {

BackupService::BackupService(Db& db, std::string data_dir)
    : db_(db), backup_dir_(std::move(data_dir) + "/backups") {}

BackupService::~BackupService() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void BackupService::start() {
    worker_ = std::thread([this] { loop(); });
}

std::string BackupService::backup_now() {
    fs::create_directories(backup_dir_);

    std::time_t now = std::time(nullptr);
    char stamp[32] = "unknown";
    if (std::tm* utc = std::gmtime(&now)) {
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", utc);
    }
    std::string path = backup_dir_ + "/broom-" + stamp + ".db";

    // SQL string literal: escape single quotes by doubling them.
    std::string escaped = path;
    for (std::size_t at = 0; (at = escaped.find('\'', at)) != std::string::npos; at += 2) {
        escaped.insert(at, 1, '\'');
    }
    db_.exec("VACUUM INTO '" + escaped + "'");

    prune();
    return path;
}

void BackupService::prune() {
    std::vector<fs::path> backups;
    for (const auto& entry : fs::directory_iterator(backup_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".db") {
            backups.push_back(entry.path());
        }
    }
    // Timestamped names sort chronologically; newest last.
    std::sort(backups.begin(), backups.end());
    while (backups.size() > static_cast<std::size_t>(kKeep)) {
        std::error_code ec;
        fs::remove(backups.front(), ec);
        backups.erase(backups.begin());
    }
}

void BackupService::loop() {
    for (;;) {
        try {
            backup_now();
        } catch (const std::exception& e) {
            // No cluster reference here by design (backups must not depend on
            // the gateway being up); stderr is the fallback channel.
            std::fprintf(stderr, "Backup failed: %s\n", e.what());
        }

        std::unique_lock lock(mutex_);
        wake_.wait_for(lock, std::chrono::hours(24), [this] { return stopping_; });
        if (stopping_) return;
    }
}

} // namespace broom
