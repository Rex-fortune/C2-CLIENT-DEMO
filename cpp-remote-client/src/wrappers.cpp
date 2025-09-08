#include "wrappers.h"
#include "sockets.h"
#include <iostream>
#include <array>
#include <cstdio>
#include <vector>
#include <filesystem>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#endif
#include "file_transfer.h"
#include "file_manager.h"
#include "process_manager.h"
#include "system_info.h"
#include "chat.h"
#include "power_control.h"
#include "clipboard.h"
#include "camera.h"
#include "screenshot.h"
#include "recording.h"
#include "help.h"
#include "security.h"

// Implementations that call existing functions in the project
std::string exec_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "Failed to run command\n";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result.empty() ? "No output\n" : result;
}

bool send_all(sock_t sock, const char* buffer, size_t length) {
    size_t total = 0;
    while (total < length) {
#ifdef _WIN32
        int sent = send(sock, buffer + total, (int)(length - total), 0);
#else
        ssize_t sent = send(sock, buffer + total, length - total, 0);
#endif
        if (sent <= 0) return false;
        total += sent;
    }
    return true;
}

bool recv_all(sock_t sock, char* buffer, size_t length) {
    size_t total = 0;
    while (total < length) {
#ifdef _WIN32
        int r = recv(sock, buffer + total, (int)(length - total), 0);
#else
        ssize_t r = recv(sock, buffer + total, length - total, 0);
#endif
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

void open_file_manager(sock_t sock) {
    namespace fs = std::filesystem;
    std::string listing;
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        listing += entry.path().string() + "\n";
    }
    send_all(sock, listing.c_str(), listing.size());
}

void open_process_manager(sock_t sock) {
#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        std::string res = "Failed to get process snapshot\n";
        send_all(sock, res.c_str(), res.size());
        return;
    }
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    std::string result;
    if (Process32First(hSnap, &pe32)) {
        do {
            result += std::string(pe32.szExeFile) + " PID: " + std::to_string(pe32.th32ProcessID) + "\n";
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    send_all(sock, result.c_str(), result.size());
#else
    std::string result;
    FILE* pipe = popen("ps -e -o pid,comm", "r");
    if (!pipe) result = "Failed to run ps\n";
    else {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) result += buf;
        pclose(pipe);
    }
    send_all(sock, result.c_str(), result.size());
#endif
}

void send_system_info(sock_t sock) {
    std::string info = get_system_info();
    send_all(sock, info.c_str(), info.size());
}

void start_chat(sock_t sock) {
    chat_interface(sock);
}

void handle_power_control(sock_t sock) {
    // This will call the power_control functions as per implementation
    PowerControl::restart();
    std::string msg = "Power control: restart issued\n";
    send_all(sock, msg.c_str(), msg.size());
}

void sync_clipboard(sock_t sock) {
    // Protocol: receive length then text
    uint32_t len_net = 0;
    if (!recv_all(sock, reinterpret_cast<char*>(&len_net), sizeof(len_net))) return;
    uint32_t len = ntohl(len_net);
    if (len == 0) return;
    std::string text(len, '\0');
    if (!recv_all(sock, &text[0], len)) return;
    set_clipboard_text(text);
    std::string ack = "CLIPBOARD_OK\n";
    send_all(sock, ack.c_str(), ack.size());
}

void access_camera(sock_t sock) {
    // camera.cpp defines access_camera() with no args; call it
    ::access_camera();
}

void manage_sessions(sock_t sock) {
    std::string msg = "Session management not implemented yet\n";
    send_all(sock, msg.c_str(), msg.size());
}

void handle_file_transfer(sock_t sock) {
    std::string msg = "File transfer not implemented yet\n";
    send_all(sock, msg.c_str(), msg.size());
}

void take_screenshot(sock_t sock) {
    std::vector<unsigned char> img;
    if (capture_screenshot_png(img)) {
        send_all(sock, reinterpret_cast<const char*>(img.data()), img.size());
    } else {
        std::string msg = "Screenshot failed\n";
        send_all(sock, msg.c_str(), msg.size());
    }
}

void start_recording(sock_t sock) {
    std::string msg = "Session recording not implemented yet\n";
    send_all(sock, msg.c_str(), msg.size());
}

void show_help(sock_t sock) {
    std::string help = "Help: see documentation or contact admin.\n";
    send_all(sock, help.c_str(), help.size());
    display_help();
}

void handle_security(sock_t sock) {
    std::string msg = "Security features not implemented yet\n";
    send_all(sock, msg.c_str(), msg.size());
}
