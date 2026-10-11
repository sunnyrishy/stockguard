#include "stockguard/database.hpp"
namespace stockguard {
namespace {
void check(int rc, sqlite3 *db) {
    if (rc != SQLITE_OK)
        throw Error("database", sqlite3_errmsg(db));
}
} // namespace
Statement::Statement(sqlite3 *db, const std::string &sql) {
    check(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement_, nullptr), db);
}
Statement::~Statement() { sqlite3_finalize(statement_); }
void Statement::bind(int i, const std::string &v) {
    check(sqlite3_bind_text(statement_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT),
          sqlite3_db_handle(statement_));
}
void Statement::bind(int i, Integer v) {
    check(sqlite3_bind_int64(statement_, i, v), sqlite3_db_handle(statement_));
}
bool Statement::step() {
    const int rc = sqlite3_step(statement_);
    if (rc == SQLITE_ROW)
        return true;
    if (rc == SQLITE_DONE)
        return false;
    throw Error("database", sqlite3_errmsg(sqlite3_db_handle(statement_)));
}
Integer Statement::integer(int c) const { return sqlite3_column_int64(statement_, c); }
std::string Statement::text(int c) const {
    const auto *text = sqlite3_column_text(statement_, c);
    return text ? std::string(reinterpret_cast<const char *>(text),
                              static_cast<std::size_t>(sqlite3_column_bytes(statement_, c)))
                : "";
}
Database::Database(const std::string &path) {
    const int rc = sqlite3_open_v2(
        path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        std::string message = db_ ? sqlite3_errmsg(db_) : "Cannot allocate SQLite connection";
        sqlite3_close(db_);
        db_ = nullptr;
        throw Error("database", message);
    }
    try {
        check(sqlite3_busy_timeout(db_, 5000), db_);
        exec("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;");
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
Database::~Database() { sqlite3_close(db_); }
void Database::exec(const std::string &sql) {
    check(sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr), db_);
}
Statement Database::prepare(const std::string &sql) { return Statement(db_, sql); }
int Database::changes() const { return sqlite3_changes(db_); }
Transaction::Transaction(Database &db) : database_(db) { database_.exec("BEGIN IMMEDIATE"); }
Transaction::~Transaction() {
    if (!committed_) {
        try {
            database_.exec("ROLLBACK");
        } catch (...) {
        }
    }
}
void Transaction::commit() {
    database_.exec("COMMIT");
    committed_ = true;
}
} // namespace stockguard
