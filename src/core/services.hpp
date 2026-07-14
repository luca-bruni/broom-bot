#pragma once

#include "core/catalog.hpp"

#include <chrono>
#include <cstdint>
#include <string>

namespace broom {

class Db;
class JobRunner;

// Shared infrastructure handed to commands that need more than the DPP event
// (e.g. to enqueue background jobs, read the database, or describe the bot).
// Owned by main; outlives the command registry. Commands that don't need it
// ignore it.
struct Services {
    JobRunner& jobs;
    Db& db;
    CommandCatalog& catalog;
    std::chrono::steady_clock::time_point started_at;
    std::string version;

    // Whole seconds since the bot started (for /about and /stats).
    std::int64_t uptime_seconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - started_at)
            .count();
    }
};

} // namespace broom
