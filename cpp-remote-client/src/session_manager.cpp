#include "session_manager.h"
#include <iostream>
#include <algorithm>

Session::Session(const std::string& id)
    : sessionId(id), active(false) {}

void Session::start() { active = true; }

void Session::stop() { active = false; }

std::string Session::getId() const { return sessionId; }

SessionManager::SessionManager() {}

SessionManager::~SessionManager() {}

void SessionManager::createSession(const std::string& sessionId) {
    sessions.emplace_back(sessionId);
    std::cout << "Session created: " << sessionId << std::endl;
}

void SessionManager::terminateSession(const std::string& sessionId) {
    auto it = std::remove_if(sessions.begin(), sessions.end(), [&sessionId](const Session& s){ return s.getId() == sessionId; });
    if (it != sessions.end()) {
        sessions.erase(it, sessions.end());
        std::cout << "Session terminated: " << sessionId << std::endl;
    }
}

void SessionManager::listSessions() const {
    std::cout << "Active sessions:" << std::endl;
    for (const auto& s : sessions) std::cout << s.getId() << std::endl;
}

Session* SessionManager::getSession(const std::string& sessionId) {
    for (auto& s : sessions) {
        if (s.getId() == sessionId) return &s;
    }
    return nullptr;
}