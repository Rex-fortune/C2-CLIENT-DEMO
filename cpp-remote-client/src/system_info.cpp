#include "system_info.h"
#include <iostream>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <sysinfoapi.h>
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#endif
#include <string>
#include <vector>

std::string get_system_info() {
    std::string out;
#if defined(_WIN32) || defined(_WIN64)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        unsigned long long uptime_ms = GetTickCount64();
        out += "Uptime: "; out += std::to_string(uptime_ms / 1000); out += " seconds\n";
        out += "Total RAM: "; out += std::to_string(memInfo.ullTotalPhys / (1024 * 1024)); out += " MB\n";
        out += "Available RAM: "; out += std::to_string(memInfo.ullAvailPhys / (1024 * 1024)); out += " MB\n";
    } else {
        out += "Error retrieving memory information.\n";
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        out += "Uptime: "; out += std::to_string(info.uptime); out += " seconds\n";
        out += "Total RAM: "; out += std::to_string(info.totalram / (1024 * 1024)); out += " MB\n";
        out += "Free RAM: "; out += std::to_string(info.freeram / (1024 * 1024)); out += " MB\n";
        out += "Total Swap: "; out += std::to_string(info.totalswap / (1024 * 1024)); out += " MB\n";
        out += "Free Swap: "; out += std::to_string(info.freeswap / (1024 * 1024)); out += " MB\n";
        out += "Number of Processes: "; out += std::to_string(info.procs); out += "\n";
    } else {
        out += "Error retrieving system information.\n";
    }
#endif
    return out;
}

// Minimal wrappers for other header-declared functions
std::string get_cpu_info() {
    std::string cpu_info;
    FILE* file = popen("lscpu", "r");
    if (file) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), file) != nullptr) cpu_info += buffer;
        pclose(file);
    }
    return cpu_info;
}

std::string get_disk_info() {
    std::string disk_info;
    FILE* file = popen("df -h", "r");
    if (file) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), file) != nullptr) disk_info += buffer;
        pclose(file);
    }
    return disk_info;
}