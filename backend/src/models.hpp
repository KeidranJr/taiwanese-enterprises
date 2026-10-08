// models.hpp
// Simple data classes for the Taiwanese Enterprises LLC backend.
// Written at a COP 3330 level: plain classes, constructors, getters,
// and a toJson helper so each object can turn itself into a JSON value.

#pragma once

#include <string>
#include <vector>
#include "../third_party/crow.h"

// A service category shown on the website, for example Transportation.
class Service {
public:
    Service(const std::string& name,
            const std::string& description,
            const std::vector<std::string>& items)
        : name_(name), description_(description), items_(items) {}

    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    const std::vector<std::string>& items() const { return items_; }

    // Turn this service into a JSON object for the /api/services route.
    crow::json::wvalue toJson() const {
        crow::json::wvalue j;
        j["name"] = name_;
        j["description"] = description_;
        for (size_t i = 0; i < items_.size(); i++) {
            j["items"][i] = items_[i];
        }
        return j;
    }

private:
    std::string name_;
    std::string description_;
    std::vector<std::string> items_;
};

// One quote request submitted through the website form.
class QuoteRequest {
public:
    QuoteRequest() : id_(0) {}

    QuoteRequest(const std::string& name,
                 const std::string& phone,
                 const std::string& email,
                 const std::string& address,
                 const std::string& service,
                 const std::string& details)
        : id_(0), name_(name), phone_(phone), email_(email),
          address_(address), service_(service), details_(details) {}

    long long id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& phone() const { return phone_; }
    const std::string& email() const { return email_; }
    const std::string& address() const { return address_; }
    const std::string& service() const { return service_; }
    const std::string& details() const { return details_; }
    const std::string& createdAt() const { return created_at_; }

    // Set by QuoteStore after the row is inserted.
    void setId(long long id) { id_ = id; }
    void setCreatedAt(const std::string& t) { created_at_ = t; }

    // A quote is valid when the required fields are filled in.
    // Email is optional, but when given it must look like an email.
    bool isValid(std::string& whyNot) const {
        if (name_.empty()) { whyNot = "name is required"; return false; }
        if (phone_.empty()) { whyNot = "phone is required"; return false; }
        if (address_.empty()) { whyNot = "service address is required"; return false; }
        if (service_.empty()) { whyNot = "service is required"; return false; }
        if (!email_.empty()) {
            size_t at = email_.find('@');
            size_t dot = email_.rfind('.');
            if (at == std::string::npos || dot == std::string::npos || dot < at) {
                whyNot = "email does not look valid";
                return false;
            }
        }
        return true;
    }

    crow::json::wvalue toJson() const {
        crow::json::wvalue j;
        j["id"] = id_;
        j["name"] = name_;
        j["phone"] = phone_;
        j["email"] = email_;
        j["address"] = address_;
        j["service"] = service_;
        j["details"] = details_;
        j["created_at"] = created_at_;
        return j;
    }

private:
    long long id_;
    std::string name_;
    std::string phone_;
    std::string email_;
    std::string address_;
    std::string service_;
    std::string details_;
    std::string created_at_;
};

// The four service categories shown on the site.
// Shared by main.cpp so the API and the website always agree.
inline std::vector<Service> buildServiceList() {
    return {
        Service("Transportation",
                "Hot shot, hauling, and vehicle moves",
                {"Hot shot & freight", "Hauling", "Vehicle transport", "Towing"}),
        Service("Cleaning Services",
                "Make it look new again",
                {"Pressure washing", "Garbage can cleaning", "Yard cleanup"}),
        Service("Event Services",
                "Setup and breakdown handled for you",
                {"Tables & chairs", "Event setup", "Breakdown & cleanup"}),
        Service("Property Services",
                "Clear it out and clean it up",
                {"Junk removal", "Debris removal", "General cleanup"}),
    };
}
