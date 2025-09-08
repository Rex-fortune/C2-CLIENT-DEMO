#include "chat.h"
#include "sockets.h"
#include "wrappers.h"
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cstring>

class ChatClient {
public:
    ChatClient(sock_t socket) : sock(socket), stop(false) {
        std::thread(&ChatClient::receive_messages, this).detach();
    }

    ~ChatClient() {
        stop = true;
        cv.notify_all();
    }

    void send_message(const std::string& message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (send_all(sock, message.c_str(), message.size())) {
            std::cout << "You: " << message << std::endl;
        } else {
            std::cerr << "Failed to send message." << std::endl;
        }
    }

    void receive_messages() {
        char buffer[1024];
        while (!stop) {
            memset(buffer, 0, sizeof(buffer));
            int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (len > 0) {
                std::lock_guard<std::mutex> lock(mtx);
                std::string message(buffer, len);
                std::cout << "Server: " << message << std::endl;
            } else {
                std::cerr << "Connection closed or error occurred." << std::endl;
                break;
            }
        }
    }

private:
    sock_t sock;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;
};

void chat_interface(sock_t sock) {
    ChatClient chatClient(sock);
    std::string message;

    while (true) {
        std::getline(std::cin, message);
        if (message == "exit" || message == "EXIT") {
            break;
        }
        chatClient.send_message(message);
    }
}