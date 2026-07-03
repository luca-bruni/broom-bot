#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace broom {

// Thin RAII wrapper over the vendored SQLite. Opened in serialized threading
// mode, so one Db instance may be used from multiple threads.
class Db {
public:
    class Stmt {
    public:
        Stmt(sqlite3* db, const std::string& sql);
        ~Stmt();
        Stmt(const Stmt&) = delete;
        Stmt& operator=(const Stmt&) = delete;

        Stmt& bind(int index, std::int64_t value);
        Stmt& bind(int index, const std::string& value);
        Stmt& bind_null(int index);

        // Advances one row; false when done.
        bool step();

        std::int64_t column_int(int index) const;
        std::string column_text(int index) const;
        bool column_is_null(int index) const;

    private:
        sqlite3_stmt* stmt_ = nullptr;
    };

    // Opens (creating if needed) with WAL journaling and foreign keys on.
    // Throws std::runtime_error on failure.
    explicit Db(const std::string& path);
    ~Db();
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    void exec(const std::string& sql);
    Stmt prepare(const std::string& sql) { return Stmt(handle_, sql); }
    std::int64_t last_insert_id() const;

    // Runs each schema step whose index is >= PRAGMA user_version, then bumps
    // user_version. Steps must be append-only across releases.
    void migrate(const std::vector<std::string>& steps);

private:
    sqlite3* handle_ = nullptr;
};

} // namespace broom
