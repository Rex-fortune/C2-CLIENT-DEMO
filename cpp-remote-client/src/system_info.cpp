#include "system_info.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <array>
#include <sstream>
#include "json.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <sysinfoapi.h>
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <string>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

std::string get_real_ip_address() {
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
                if (strcmp(ip, "127.0.0.1") != 0) {
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




// Helper to run shell commands and capture output
std::string run_command(const char* cmd) {
    std::array<char, 128> buffer{};
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

json get_system_info_json() {
    
    json info;

  
        // CPU
#if defined(_WIN32) || defined(_WIN64)
        SYSTEM_INFO sysInfo{};
        GetSystemInfo(&sysInfo);
        info["cpu"] = "Processors: " + std::to_string(sysInfo.dwNumberOfProcessors);
#else
        std::string cpu = run_command("lscpu | grep 'Model name' | head -1 | awk -F: '{print $2}'");
        if (cpu.empty()) cpu = "Unknown CPU";
        info["cpu"] = cpu;
#endif

        // RAM
#if defined(_WIN32) || defined(_WIN64)
        MEMORYSTATUSEX memInfo{};
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            info["ram"] = std::to_string(memInfo.ullTotalPhys / (1024 * 1024)) + " MB";
        } else {
            info["ram"] = "N/A";
        }
#else
        struct sysinfo sys{};
        if (sysinfo(&sys) == 0) {
            info["ram"] = std::to_string(sys.totalram / (1024 * 1024)) + " MB";
        } else {
            info["ram"] = "N/A";
        }
#endif

        // Disk
#if defined(_WIN32) || defined(_WIN64)
        ULARGE_INTEGER freeBytesAvailable{}, totalBytes{}, totalFree{};
        if (GetDiskFreeSpaceEx("C:\\", &freeBytesAvailable, &totalBytes, &totalFree)) {
            info["disk"] = std::to_string(totalBytes.QuadPart / (1024ULL * 1024 * 1024)) + " GB";
        } else {
            info["disk"] = "N/A";
        }
#else
        std::string disk = run_command("df -h --total | grep total | awk '{print $2}'");
        if (disk.empty()) disk = "Unknown Disk Size";
        info["disk"] = disk;
#endif

        // OS
#if defined(_WIN32) || defined(_WIN64)
        info["os"] = "Windows";
#else
        std::string os = run_command("uname -s -r");
        if (os.empty()) os = "Windows";
        info["os"] = os;
#endif

        // Network (IP address)
#if defined(_WIN32) || defined(_WIN64)
       

        info["ip"] = get_real_ip_address(); // Avoid calling Windows API under Wine
#else
        std::string ip = run_command("hostname -I | awk '{print $1}'");
        if (ip.empty()) ip = "Unknown IP";
        info["ip"] = ip;
#endif

   

    return info;
}
