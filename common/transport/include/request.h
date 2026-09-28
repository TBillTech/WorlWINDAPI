#pragma once
#include <string>
#include <iostream>

struct Request {
    // Example URI: "https://www.example.com:443/path/to/resource?query=example#fragment"
    std::string scheme;  // Example: "https"
    std::string authority; // Example: "www.example.com:443"
    std::string path; // Example: "/path/to/resource"
    std::string method; // Example: "GET", "POST", etc.
    struct {
        int32_t urgency;
        int inc;
    } pri;

    // Operator < needs to compare all fields, but path should be most important:
    bool operator<(const Request &req) const {
        if (path < req.path) {
            return true;
        }
        if (path > req.path) {
            return false;
        }
        if (authority < req.authority) {
            return true;
        }
        if (authority > req.authority) {
            return false;
        }
        if (scheme < req.scheme) {
            return true;
        }
        if (scheme > req.scheme) {
            return false;
        }
        if (method < req.method) {
            return true;
        }
        if (method > req.method) {
            return false;
        }
        if (pri.urgency < req.pri.urgency) {
            return true;
        }
        if (pri.urgency > req.pri.urgency) {
            return false;
        }
        return pri.inc < req.pri.inc;
    }
    bool operator==(const Request &req) const {
        return scheme == req.scheme && authority == req.authority &&
            path == req.path && method == req.method && pri.urgency == req.pri.urgency &&
            pri.inc == req.pri.inc;
    }
    friend std::ostream& operator<<(std::ostream& os, const Request& req) {
        os << "Request: scheme=" << req.scheme << ", authority=" << req.authority << ", path=" << req.path << ", method=" << req.method<< ", urgency=" << req.pri.urgency << ", inc=" << req.pri.inc;
        return os;
    }
    bool isWWATP() const {
        return path.find("wwatp/") != std::string::npos;
    }
};
