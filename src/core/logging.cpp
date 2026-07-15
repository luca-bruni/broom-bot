#include "core/logging.hpp"

#include <ctime>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace broom {

FileLogger::FileLogger(std::string path) : path_(std::move(path)) {
    fs::path parent = fs::path(path_).parent_path();
    if (!parent.empty()) fs::create_directories(parent);
    std::error_code ec;
    written_ = static_cast<std::size_t>(fs::file_size(path_, ec));
    if (ec) written_ = 0;
    file_.open(path_, std::ios::app);
}

void FileLogger::operator()(const dpp::log_t& event) {
    std::lock_guard lock(mutex_);
    if (!file_.is_open()) return;

    std::time_t now = std::time(nullptr);
    char stamp[24] = "????-??-?? ??:??:??";
    if (std::tm* utc = std::gmtime(&now)) {
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", utc);
    }

    std::string line = std::string(stamp) + " [" + dpp::utility::loglevel(event.severity) +
                       "] " + event.message + "\n";
    file_ << line;
    file_.flush();
    written_ += line.size();
    if (written_ > kMaxBytes) rotate();
}

void FileLogger::rotate() {
    file_.close();
    std::error_code ec;
    fs::remove(path_ + "." + std::to_string(kKeep), ec);
    for (int i = kKeep - 1; i >= 1; --i) {
        fs::rename(path_ + "." + std::to_string(i), path_ + "." + std::to_string(i + 1), ec);
    }
    fs::rename(path_, path_ + ".1", ec);
    file_.open(path_, std::ios::trunc);
    written_ = 0;
}

} // namespace broom
