#include "shell.h"
#include <iostream>
#include <string>
#include <vector>

RemoteShell::RemoteShell() {}

RemoteShell::~RemoteShell() {}

void RemoteShell::executeCommand(const std::string& command) {
    // Simple local execution placeholder
    std::string output;
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    currentOutput = output;
}

std::string RemoteShell::getOutput() { return currentOutput; }

void RemoteShell::clearOutput() { currentOutput.clear(); }