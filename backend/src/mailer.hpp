// mailer.hpp
// Sends "new quote request" emails through Gmail's SMTP server.
// Written at a COP 3330 level: one small class, constructor reads
// config, one method does the work, no exceptions escape.
//
// Setup (done once by the owner):
//   1. Turn on 2-Step Verification for the Gmail account.
//   2. Create an App Password: Google Account > Security > App passwords.
//   3. Set these environment variables where the server runs:
//        GMAIL_USER          the Gmail address, e.g. taiwanenterprisellc@gmail.com
//        GMAIL_APP_PASSWORD  the 16 letter app password (not the login password)
//        NOTIFY_TO           where quote alerts go (defaults to GMAIL_USER)
//
// If GMAIL_USER or GMAIL_APP_PASSWORD is missing, the notifier stays
// disabled and the server runs fine without sending email.

#pragma once

#include <string>
#include "models.hpp"

class EmailNotifier {
public:
    EmailNotifier();

    bool enabled() const { return enabled_; }

    // Email the owner about a new quote. Never throws.
    // Returns true when Gmail accepted the message.
    // It is fine to call this on a background thread.
    bool sendQuoteNotification(const QuoteRequest& q);

private:
    bool enabled_;
    std::string user_;
    std::string pass_;
    std::string to_;
};
