// main.cpp
// Taiwanese Enterprises LLC backend server.
// Serves the static website and exposes a small JSON API:
//   GET  /api/services   the service list
//   POST /api/quote      submit a quote request, saved in SQLite
//   GET  /api/quotes     list stored quotes (simple admin view)
//
// Config through environment variables:
//   PORT        port to listen on, default 8080
//   STATIC_DIR  folder with index.html etc, default is the repo root
//               found next to this binary (binary lives in backend/build)
//   QUOTES_DB   SQLite file path, default ./quotes.db

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <limits.h>

#include "../third_party/crow.h"
#include "models.hpp"
#include "quote_store.hpp"
#include "mailer.hpp"
#include <curl/curl.h>

namespace {

std::string getEnv(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : fallback;
}

// Folder that holds this running program, so we can find the
// website files relative to it no matter where it is started from.
std::string exeDir() {
    char buf[PATH_MAX] = {0};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    std::string p(buf, n > 0 ? (size_t)n : 0);
    size_t slash = p.rfind('/');
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

std::string contentType(const std::string& path) {
    size_t dot = path.rfind('.');
    std::string ext = (dot == std::string::npos) ? "" : path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".webp") return "image/webp";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    return "application/octet-stream";
}

// Read a whole file into a string. Empty string means it failed.
std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

crow::response jsonResponse(int code, const crow::json::wvalue& j) {
    crow::response res(code);
    res.set_header("Content-Type", "application/json");
    res.write(j.dump());
    return res;
}

// Read a string field from a JSON body. Returns "" when the
// field is missing or is not a string, so a bad client cannot
// crash the server by sending a number where text belongs.
std::string strField(const crow::json::rvalue& body, const char* key) {
    if (body.has(key) && body[key].t() == crow::json::type::String)
        return std::string(body[key].s());
    return "";
}

} // namespace

int main() {
    const std::string staticDir =
        getEnv("STATIC_DIR", exeDir() + "/../..");
    const std::string dbPath = getEnv("QUOTES_DB", "./quotes.db");
    const int port = std::stoi(getEnv("PORT", "8080"));

    QuoteStore store(dbPath);
    if (!store.ok()) {
        std::cerr << "Could not open quote database: " << dbPath << "\n";
        return 1;
    }

    // Reads GMAIL_USER / GMAIL_APP_PASSWORD / NOTIFY_TO from the
    // environment. Stays silent when they are not set.
    EmailNotifier notifier;

    // One time libcurl setup. Must run before any thread sends email.
    curl_global_init(CURL_GLOBAL_DEFAULT);

    crow::SimpleApp app;

    // Homepage.
    CROW_ROUTE(app, "/")([&] {
        std::string body = readFile(staticDir + "/index.html");
        if (body.empty()) return crow::response(404, "index.html not found");
        crow::response res(200);
        res.set_header("Content-Type", "text/html");
        res.write(body);
        return res;
    });

    // NOTE about route order: in Crow, when two routes could match the
    // same URL, the one defined FIRST wins. So every /api route must be
    // defined before the /<path> catch all below, or the catch all
    // would swallow the API calls.

    // Service list as JSON.
    CROW_ROUTE(app, "/api/services")([] {
        crow::json::wvalue j;
        auto services = buildServiceList();
        for (size_t i = 0; i < services.size(); i++) {
            j[i] = services[i].toJson();
        }
        return jsonResponse(200, j);
    });

    // Take a quote request, validate it, store it, return its id.
    CROW_ROUTE(app, "/api/quote")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
            auto body = crow::json::load(req.body);
            if (!body) {
                crow::json::wvalue e;
                e["ok"] = false;
                e["error"] = "body must be JSON";
                return jsonResponse(400, e);
            }
            QuoteRequest q(
                strField(body, "name"),
                strField(body, "phone"),
                strField(body, "email"),
                strField(body, "service"),
                strField(body, "details"));

            std::string whyNot;
            if (!q.isValid(whyNot)) {
                crow::json::wvalue e;
                e["ok"] = false;
                e["error"] = whyNot;
                return jsonResponse(400, e);
            }

            long long id = 0;
            if (!store.addQuote(q, id)) {
                crow::json::wvalue e;
                e["ok"] = false;
                e["error"] = "could not save quote";
                return jsonResponse(500, e);
            }
            // Email the owner on a background thread so the web
            // response stays fast even if Gmail is slow. A copy of
            // the quote goes with the thread, nothing shared.
            std::thread([notifier, q]() mutable {
                if (!notifier.sendQuoteNotification(q)) {
                    std::cerr << "quote email not sent for "
                              << q.name() << "\n";
                }
            }).detach();
            crow::json::wvalue ok;
            ok["ok"] = true;
            ok["id"] = id;
            return jsonResponse(200, ok);
        });

    // Stored quotes for the owner.
    // NOTE: this is open for now. Before putting real customer data
    // through it, add a login or a secret token check here.
    CROW_ROUTE(app, "/api/quotes")([&] {
        auto quotes = store.allQuotes();
        crow::json::wvalue j;
        for (size_t i = 0; i < quotes.size(); i++) {
            j[i] = quotes[i].toJson();
        }
        return jsonResponse(200, j);
    });

    // Every other static file: css, js, images.
    // <path> in Crow matches across slashes, so images/web/x.webp works.
    // This must stay AFTER the /api routes. See the note above.
    CROW_ROUTE(app, "/<path>")([&](const crow::request& req, std::string path) {
        (void)req;
        if (path.find("..") != std::string::npos) {
            return crow::response(403, "forbidden");
        }
        if (path.empty() || path.back() == '/') path += "index.html";
        std::string body = readFile(staticDir + "/" + path);
        if (body.empty()) return crow::response(404, "not found");
        crow::response res(200);
        res.set_header("Content-Type", contentType(path));
        res.write(body);
        return res;
    });

    std::cout << "Taiwanese Enterprises backend on port " << port << "\n";
    std::cout << "Serving files from " << staticDir << "\n";
    app.port(port).multithreaded().run();
    return 0;
}
