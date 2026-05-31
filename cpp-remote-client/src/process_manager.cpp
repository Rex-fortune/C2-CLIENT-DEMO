#include "process_manager.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

std::vector<ProcessInfo> get_running_processes() {
    std::vector<ProcessInfo> processes;

#ifdef _WIN32
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return processes;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            ProcessInfo pInfo;
            pInfo.pid = pe32.th32ProcessID;
            pInfo.name = pe32.szExeFile;
            processes.push_back(pInfo);
        } while (Process32Next(hProcessSnap, &pe32));
    }
    CloseHandle(hProcessSnap);
#else
    DIR* dir = opendir("/proc");
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            int pid = atoi(entry->d_name);
            if (pid > 0) {
                ProcessInfo pInfo;
                pInfo.pid = pid;

                std::ostringstream oss;
                oss << "/proc/" << pid << "/comm";
                FILE* file = fopen(oss.str().c_str(), "r");
                if (file) {
                    char buffer[256];
                    fgets(buffer, sizeof(buffer), file);
                    pInfo.name = buffer;
                    pInfo.name.erase(pInfo.name.find_last_not_of(" \n") + 1); // Trim newline
                    fclose(file);
                    processes.push_back(pInfo);
                }
            }
        }
    }
    closedir(dir);
#endif

    return processes;
}

bool terminate_process(int pid) {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL) return false;
    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    return result != 0;
#else
    return kill(pid, SIGTERM) == 0;
#endif
}

void display_processes() {
    std::vector<ProcessInfo> processes = get_running_processes();
    for (const auto& process : processes) {
        std::cout << "PID: " << process.pid << " Name: " << process.name << std::endl;
    }
}