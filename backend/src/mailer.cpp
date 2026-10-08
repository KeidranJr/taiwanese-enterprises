// mailer.cpp
// EmailNotifier implementation using libcurl's SMTP support.
// Talking to Gmail looks like this:
//   connect to smtp.gmail.com on port 587, upgrade to TLS,
//   log in with the Gmail address + app password,
//   hand over one plain text message.

#include "mailer.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <curl/curl.h>

namespace {

// What libcurl still has to send, and how much of it is already gone.
struct Payload {
    const char* data;
    size_t left;
};

// libcurl calls this whenever it wants the next chunk of the message.
size_t payloadReader(char* ptr, size_t size, size_t nmemb, void* userp) {
    Payload* p = static_cast<Payload*>(userp);
    size_t room = size * nmemb;
    if (p->left == 0) return 0; // nothing left, message is complete
    size_t n = p->left < room ? p->left : room;
    std::memcpy(ptr, p->data, n);
    p->data += n;
    p->left -= n;
    return n;
}

std::string getEnvStr(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

} // namespace

EmailNotifier::EmailNotifier() : enabled_(false) {
    user_ = getEnvStr("GMAIL_USER");
    pass_ = getEnvStr("GMAIL_APP_PASSWORD");
    to_ = getEnvStr("NOTIFY_TO");
    if (to_.empty()) to_ = user_;
    enabled_ = !user_.empty() && !pass_.empty();
    if (!enabled_) {
        std::cerr << "EmailNotifier disabled: set GMAIL_USER and "
                     "GMAIL_APP_PASSWORD to get quote emails.\n";
    }
}

bool EmailNotifier::sendQuoteNotification(const QuoteRequest& q) {
    if (!enabled_) return false;

    // The raw message: headers, a blank line, then the body.
    // Lines end with \r\n because that is what SMTP expects.
    std::string msg =
        "To: " + to_ + "\r\n"
        "From: Taiwanese Enterprises <" + user_ + ">\r\n"
        "Subject: New quote request: " + q.service() + " from " + q.name() + "\r\n"
        "\r\n"
        "New quote request from your website:\r\n"
        "\r\n"
        "Name:    " + q.name() + "\r\n"
        "Phone:   " + q.phone() + "\r\n"
        "Email:   " + (q.email().empty() ? "(none given)" : q.email()) + "\r\n"
        "Service: " + q.service() + "\r\n"
        "Details: " + (q.details().empty() ? "(none)" : q.details()) + "\r\n";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "EmailNotifier: curl_easy_init failed\n";
        return false;
    }

    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, to_.c_str());

    Payload payload{msg.c_str(), msg.size()};
    char errbuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_USERNAME, user_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, pass_.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, user_.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payloadReader);
    curl_easy_setopt(curl, CURLOPT_READDATA, &payload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    bool ok = (res == CURLE_OK);
    if (!ok) {
        std::cerr << "EmailNotifier: send failed: "
                  << (errbuf[0] ? errbuf : curl_easy_strerror(res)) << "\n";
    }

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    return ok;
}
