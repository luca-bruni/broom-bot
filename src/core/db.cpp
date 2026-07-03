#include "core/db.hpp"

#include <sqlite3.h>

#include <vector>

namespace broom {

namespace {

[[noreturn]] void fail(sqlite3* db, const std::string& what) {
    throw std::runtime_error(what + ": " + (db ? sqlite3_errmsg(db) : "unknown"));
}

} // namespace

Db::Stmt::Stmt(sqlite3* db, const std::string& sql) {
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
        fail(db, "prepare failed for: " + sql);
    }
}

Db::Stmt::~Stmt() {
    sqlite3_finalize(stmt_);
}

Db::Stmt& Db::Stmt::bind(int index, std::int64_t value) {
    sqlite3_bind_int64(stmt_, index, value);
    return *this;
}

Db::Stmt& Db::Stmt::bind(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
    return *this;
}

Db::Stmt& Db::Stmt::bind_null(int index) {
    sqlite3_bind_null(stmt_, index);
    return *this;
}

bool Db::Stmt::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    fail(sqlite3_db_handle(stmt_), "step failed");
}

std::int64_t Db::Stmt::column_int(int index) const {
    return sqlite3_column_int64(stmt_, index);
}

std::string Db::Stmt::column_text(int index) const {
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    return text ? reinterpret_cast<const char*>(text) : "";
}

bool Db::Stmt::column_is_null(int index) const {
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

Db::Db(const std::string& path) {
    if (sqlite3_open_v2(path.c_str(), &handle_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                            SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        fail(handle_, "cannot open database " + path);
    }
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");
    exec("PRAGMA busy_timeout=5000");
}

Db::~Db() {
    sqlite3_close(handle_);
}

void Db::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string message = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("exec failed: " + message + " in: " + sql);
    }
}

std::int64_t Db::last_insert_id() const {
    return sqlite3_last_insert_rowid(handle_);
}

void Db::migrate(const std::vector<std::string>& steps) {
    auto version_stmt = prepare("PRAGMA user_version");
    version_stmt.step();
    auto version = static_cast<std::size_t>(version_stmt.column_int(0));

    for (std::size_t i = version; i < steps.size(); ++i) {
        exec("BEGIN");
        exec(steps[i]);
        exec("PRAGMA user_version = " + std::to_string(i + 1));
        exec("COMMIT");
    }
}

} // namespace broom
