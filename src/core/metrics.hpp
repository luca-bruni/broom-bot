#pragma once

#include "core/db.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace broom {

// Command usage counters. Written by the registry's usage hook (wired in
// main), read by /stats. One row per command name, monotonically increasing.

inline const std::vector<std::string>& metrics_schema() {
    static const std::vector<std::string> steps = {
        R"sql(
        CREATE TABLE command_usage(
            name TEXT PRIMARY KEY,
            uses INTEGER NOT NULL DEFAULT 0
        );
        )sql",
    };
    return steps;
}

inline void record_command_use(Db& db, const std::string& name) {
    db.prepare(
          "INSERT INTO command_usage(name, uses) VALUES(?1, 1) "
          "ON CONFLICT(name) DO UPDATE SET uses = uses + 1")
        .bind(1, name)
        .step();
}

inline std::int64_t total_command_uses(Db& db) {
    auto stmt = db.prepare("SELECT COALESCE(SUM(uses), 0) FROM command_usage");
    stmt.step();
    return stmt.column_int(0);
}

} // namespace broom
