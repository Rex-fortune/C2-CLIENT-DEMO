#ifndef SECURITY_H
#define SECURITY_H
#pragma once
#include "sockets.h"





#include <string>

class Security {
public:
    // Initializes security features
    void init();

    // Authenticates a user with the given credentials
    bool authenticate(const std::string& username, const std::string& password);

    // Logs the session activity
    void logSessionActivity(const std::string& activity);

    // Checks if the session is secure
    bool isSessionSecure() const;

    // Cleans up security resources
    void cleanup();
};

#endif // SECURITY_H