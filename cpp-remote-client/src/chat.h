#ifndef CHAT_H
#define CHAT_H

#include <string>
#include <vector>
#include "sockets.h"

class Chat {
public:
    Chat();
    ~Chat();

    void sendMessage(const std::string& message);
    std::vector<std::string> receiveMessages();
    void displayChatWindow();
};

// C-style chat interface used elsewhere
void chat_interface(sock_t sock);

#endif // CHAT_H