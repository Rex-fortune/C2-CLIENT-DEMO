#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <vector>
#include <string>

class Session {
public:
    Session(const std::string& id);
    void start();
    void stop();
    std::string getId() const;

private:
    std::string sessionId;
    bool active;
};

class SessionManager {
public:
    SessionManager();
    ~SessionManager();
    
    void createSession(const std::string& sessionId);
    void terminateSession(const std::string& sessionId);
    void listSessions() const;
    Session* getSession(const std::string& sessionId);

private:
    std::vector<Session> sessions;
};

#endif // SESSION_MANAGER_H