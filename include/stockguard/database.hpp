#pragma once
#include "stockguard/domain.hpp"
#include <sqlite3.h>

namespace stockguard {
class Statement {
  public:
    Statement(sqlite3 *db, const std::string &sql);
    ~Statement();
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
    void bind(int index, const std::string &value);
    void bind(int index, Integer value);
    bool step();
    Integer integer(int column) const;
    std::string text(int column) const;

  private:
    sqlite3_stmt *statement_ = nullptr;
};
class Database {
  public:
    explicit Database(const std::string &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    void exec(const std::string &sql);
    Statement prepare(const std::string &sql);
    int changes() const;

  private:
    sqlite3 *db_ = nullptr;
};
class Transaction {
  public:
    explicit Transaction(Database &database);
    ~Transaction();
    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;
    void commit();

  private:
    Database &database_;
    bool committed_ = false;
};
} // namespace stockguard
