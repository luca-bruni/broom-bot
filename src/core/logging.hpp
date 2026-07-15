#pragma once

#include <dpp/dpp.h>

#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>

namespace broom {

// Rotating log-file sink, used alongside DPP's cout_logger (see main).
// Post-mortems need logs that outlive the terminal: stdout disappears when
// the bot runs as a service.
//
// Rotation: when the file exceeds kMaxBytes it is renamed to broom.log.1
// (shifting .1 -> .2, .2 -> .3, dropping the oldest), keeping kKeep archives.
class FileLogger {
public:
    static constexpr std::size_t kMaxBytes = 5 * 1024 * 1024;
    static constexpr int kKeep = 3;

    // Creates the directory if needed; e.g. FileLogger("data/logs/broom.log").
    explicit FileLogger(std::string path);

    // Thread-safe; DPP fires on_log from multiple threads.
    void operator()(const dpp::log_t& event);

private:
    void rotate();

    std::string path_;
    std::ofstream file_;
    std::size_t written_ = 0;
    std::mutex mutex_;
};

} // namespace broom
