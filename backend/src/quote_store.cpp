// quote_store.cpp
// SQLite implementation of QuoteStore. Uses prepared statements
// so user input is always bound as data, never pasted into SQL.

#include "quote_store.hpp"

QuoteStore::QuoteStore(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        db_ = nullptr;
        return;
    }
    if (!createTable()) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

QuoteStore::~QuoteStore() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool QuoteStore::createTable() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS quotes ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL,"
        " phone TEXT NOT NULL,"
        " email TEXT NOT NULL DEFAULT '',"
        " address TEXT NOT NULL DEFAULT '',"
        " service TEXT NOT NULL,"
        " details TEXT NOT NULL DEFAULT '',"
        " created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))"
        ");";
    char* err = nullptr;
    bool good = (sqlite3_exec(db_, sql, nullptr, nullptr, &err) == SQLITE_OK);
    if (err) sqlite3_free(err);
    // Databases created before the address field existed do not get the
    // new column from CREATE TABLE IF NOT EXISTS, so add it here. On a
    // fresh database this fails with "duplicate column" and that is fine.
    err = nullptr;
    sqlite3_exec(db_,
                 "ALTER TABLE quotes ADD COLUMN"
                 " address TEXT NOT NULL DEFAULT '';",
                 nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return good;
}

bool QuoteStore::addQuote(const QuoteRequest& q, long long& newId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql =
        "INSERT INTO quotes (name, phone, email, address, service, details)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, q.name().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, q.phone().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, q.email().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, q.address().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, q.service().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, q.details().c_str(), -1, SQLITE_TRANSIENT);

    bool good = (sqlite3_step(stmt) == SQLITE_DONE);
    if (good) {
        newId = sqlite3_last_insert_rowid(db_);
    }
    sqlite3_finalize(stmt);
    return good;
}

std::vector<QuoteRequest> QuoteStore::allQuotes() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QuoteRequest> out;
    if (!db_) return out;

    const char* sql =
        "SELECT id, name, phone, email, address, service, details, created_at"
        " FROM quotes ORDER BY id DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return out;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QuoteRequest q(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
        q.setId(sqlite3_column_int64(stmt, 0));
        q.setCreatedAt(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
        out.push_back(q);
    }
    sqlite3_finalize(stmt);
    return out;
}
