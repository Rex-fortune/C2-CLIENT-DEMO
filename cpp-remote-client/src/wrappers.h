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

void open_file_manager(sock_t sock);
void open_process_manager(sock_t sock);
void send_system_info(sock_t sock);
void start_chat(sock_t sock);
void handle_power_control(sock_t sock);
void sync_clipboard(sock_t sock);
void access_camera(sock_t sock);
void manage_sessions(sock_t sock);
void handle_file_transfer(sock_t sock);
void take_screenshot(sock_t sock);
void start_recording(sock_t sock);
void show_help(sock_t sock);
void handle_security(sock_t sock);

#endif // WRAPPERS_H
