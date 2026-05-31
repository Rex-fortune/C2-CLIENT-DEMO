#pragma once
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <string>
#include "json.hpp"

using json = nlohmann::json;

// Define sock_t type for cross-platform compatibility
#if defined(_WIN32) || defined(_WIN64)
    typedef SOCKET sock_t;
#else
    typedef int sock_t;
#endif

// API for screen sharing
void handle_screen_share(sock_t sock, const json& payload, const std::string& client);
static void process_screen_input_payload(sock_t sock, const json &payload, const std::string &client);
static void sharing_loop(sock_t sock, std::string client);
static void open_url_default_browser(const std::string &url);
static std::string exec_command_capture(const std::string &cmd);
