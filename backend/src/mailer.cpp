// mailer.cpp
// EmailNotifier implementation using libcurl's SMTP support.
// Talking to Gmail looks like this:
//   connect to smtp.gmail.com on port 587, upgrade to TLS,
//   log in with the Gmail address + app password,
//   hand over one plain text message.

#include "mailer.hpp"

#include <cstdlib>
#include <cstring>
#include <ctime>
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

// Escape text that goes into the HTML copy of the email, so a
// customer typing < or & in the form cannot break the layout.
std::string htmlEscape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return out;
}

// Percent-encode a value so it can sit inside a URL query string.
std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                    c == '.' || c == '~';
        if (safe) {
            out += static_cast<char>(c);
        } else if (c == ' ') {
            out += '+';
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// "October 08, 2026 at 02:41 PM" for the Received line.
std::string nowStamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmv;
    localtime_r(&t, &tmv);
    char buf[64] = {0};
    std::strftime(buf, sizeof(buf), "%B %d, %Y at %I:%M %p", &tmv);
    return std::string(buf);
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

    // The message is multipart: a plain text copy for simple mail
    // apps, plus a formatted HTML copy that reads like a real
    // request form. Both carry the same fields.
    const std::string boundary = "==TEQuoteBoundary7f3a==";
    const std::string received = nowStamp();
    const std::string mapsUrl =
        "https://www.google.com/maps/search/?api=1&query=" +
        urlEncode(q.address());

    std::string text =
        "TAIWANESE ENTERPRISES LLC\r\n"
        "New Quote Request\r\n"
        "----------------------------------------\r\n"
        "\r\n"
        "Name:            " + q.name() + "\r\n"
        "Phone:           " + q.phone() + "\r\n"
        "Email:           " + (q.email().empty() ? "(none given)" : q.email()) + "\r\n"
        "Service:         " + q.service() + "\r\n"
        "Service address: " + q.address() + "\r\n"
        "Map:             " + mapsUrl + "\r\n"
        "Received:        " + received + "\r\n"
        "\r\n"
        "Job details:\r\n" +
        (q.details().empty() ? std::string("(none given)") : q.details()) + "\r\n"
        "\r\n"
        "----------------------------------------\r\n"
        "One Company. Many Services. One Call.\r\n";

    // One table row of the HTML field list.
    auto row = [](const std::string& label, const std::string& valueHtml) {
        return std::string(
            "<tr>"
            "<td style=\"padding:8px 12px 8px 0;color:#8a8378;"
            "font-size:12px;font-weight:bold;letter-spacing:0.5px;"
            "text-transform:uppercase;vertical-align:top;"
            "white-space:nowrap;\">" + label + "</td>"
            "<td style=\"padding:8px 0;color:#22201c;font-size:15px;"
            "vertical-align:top;\">" + valueHtml + "</td>"
            "</tr>");
    };
    const std::string gold = "#c9a227";
    const std::string linkStyle =
        "color:" + gold + ";text-decoration:none;font-weight:bold;";
    std::string emailVal = q.email().empty()
        ? "(none given)"
        : "<a href=\"mailto:" + htmlEscape(q.email()) +
              "\" style=\"" + linkStyle + "\">" +
              htmlEscape(q.email()) + "</a>";
    std::string addressVal =
        htmlEscape(q.address()) +
        " &nbsp;<a href=\"" + mapsUrl + "\" style=\"" + linkStyle +
        "\">View on map</a>";

    std::string html =
        "<!DOCTYPE html><html><body style=\"margin:0;padding:0;"
        "background:#f4f1ea;\">"
        "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
        "cellspacing=\"0\" style=\"background:#f4f1ea;padding:24px 0;\">"
        "<tr><td align=\"center\">"
        "<table role=\"presentation\" width=\"600\" cellpadding=\"0\" "
        "cellspacing=\"0\" style=\"background:#ffffff;"
        "border:1px solid #e2ddd0;border-radius:8px;overflow:hidden;"
        "font-family:Arial,Helvetica,sans-serif;\">"
        // Header band
        "<tr><td style=\"background:#16130a;padding:26px 32px;\">"
        "<div style=\"color:#f5f2ea;font-size:19px;font-weight:bold;"
        "letter-spacing:1.5px;\">TAIWANESE ENTERPRISES LLC</div>"
        "<div style=\"color:#e8c55a;font-size:13px;letter-spacing:1px;"
        "margin-top:5px;\">NEW QUOTE REQUEST</div>"
        "</td></tr>"
        // Field list
        "<tr><td style=\"padding:24px 32px 8px 32px;\">"
        "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
        "cellspacing=\"0\">" +
        row("Name", htmlEscape(q.name())) +
        row("Phone", "<a href=\"tel:" + htmlEscape(q.phone()) +
              "\" style=\"" + linkStyle + "\">" +
              htmlEscape(q.phone()) + "</a>") +
        row("Email", emailVal) +
        row("Service", htmlEscape(q.service())) +
        row("Address", addressVal) +
        row("Received", htmlEscape(received)) +
        "</table>"
        "</td></tr>"
        // Job details box
        "<tr><td style=\"padding:12px 32px 24px 32px;\">"
        "<div style=\"color:#8a8378;font-size:12px;font-weight:bold;"
        "letter-spacing:0.5px;text-transform:uppercase;"
        "margin-bottom:8px;\">Job details</div>"
        "<div style=\"background:#faf8f2;border:1px solid #e8e2d2;"
        "border-radius:6px;padding:14px 16px;color:#33302a;"
        "font-size:15px;line-height:1.5;\">" +
        htmlEscape(q.details().empty() ? "(none given)" : q.details()) +
        "</div>"
        "</td></tr>"
        // Footer
        "<tr><td style=\"background:#16130a;padding:16px 32px;"
        "color:#b7b0a1;font-size:12px;text-align:center;\">"
        "One Company. Many Services. One Call.<br/>"
        "Taiwanese Enterprises LLC"
        "</td></tr>"
        "</table>"
        "</td></tr></table>"
        "</body></html>";

    // The raw message: headers, a blank line, then the two parts.
    // Lines end with \r\n because that is what SMTP expects.
    std::string msg =
        "To: " + to_ + "\r\n"
        "From: Taiwanese Enterprises <" + user_ + ">\r\n" +
        (q.email().empty()
             ? std::string()
             : "Reply-To: " + q.email() + "\r\n") +
        "Subject: New Quote Request: " + q.service() + " from " +
        q.name() + "\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/alternative; boundary=\"" + boundary +
        "\"\r\n"
        "\r\n"
        "--" + boundary + "\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "\r\n" + text +
        "\r\n--" + boundary + "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n" + html +
        "\r\n--" + boundary + "--\r\n";

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
