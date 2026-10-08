// quote_store.hpp
// QuoteStore saves quote requests in a SQLite database file.
// One SQLite connection is shared, so every method locks a mutex
// first. That keeps it safe when Crow handles requests on threads.

#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <sqlite3.h>
#include "models.hpp"

class QuoteStore {
public:
    // Opens (or creates) the database file and makes the table.
    explicit QuoteStore(const std::string& dbPath);
    ~QuoteStore();

    // No copying. One store owns one database connection.
    QuoteStore(const QuoteStore&) = delete;
    QuoteStore& operator=(const QuoteStore&) = delete;

    bool ok() const { return db_ != nullptr; }

    // Insert a quote. On success returns true and sets newId.
    bool addQuote(const QuoteRequest& q, long long& newId);

    // Read every stored quote, newest first.
    std::vector<QuoteRequest> allQuotes();

private:
    bool createTable();

    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};
