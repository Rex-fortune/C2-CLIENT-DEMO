#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <string>
#include <vector>

// Structure to hold information about a process
struct ProcessInfo {
    int pid;                // Process ID
    std::string name;      // Process name
    float cpuUsage;        // CPU usage percentage
    float memoryUsage;     // Memory usage in MB
};

// Class to manage processes on the remote machine
class ProcessManager {
public:
    // Retrieve a list of currently running processes
    std::vector<ProcessInfo> getRunningProcesses();

    // Terminate a process by its ID
    bool terminateProcess(int pid);

    // Get detailed information about a specific process
    ProcessInfo getProcessInfo(int pid);
};

#endif // PROCESS_MANAGER_H