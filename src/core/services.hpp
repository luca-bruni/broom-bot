#pragma once

#include "core/catalog.hpp"

#include <chrono>
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
};

} // namespace broom
