#pragma once

namespace broom {

class Db;
class JobRunner;

// Shared infrastructure handed to commands that need more than the DPP event
// (e.g. to enqueue background jobs or read the database). Owned by main;
// outlives the command registry. Commands that don't need it ignore it.
struct Services {
    JobRunner& jobs;
    Db& db;
};

} // namespace broom
