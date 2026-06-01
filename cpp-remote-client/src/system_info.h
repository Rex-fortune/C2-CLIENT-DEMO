#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <string>

// Function to get CPU information
std::string get_cpu_info();

// Function to get RAM information
std::string get_ram_info();

// Function to get disk space information
std::string get_disk_space_info();

// Function to get OS version
std::string get_os_version();

// Function to get network information
std::string get_network_info();

// Function to gather and return all system information as a formatted string
std::string get_system_info();

#endif // SYSTEM_INFO_H