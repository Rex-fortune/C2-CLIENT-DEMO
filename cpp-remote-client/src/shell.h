#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>

class RemoteShell {
public:
    RemoteShell();
    ~RemoteShell();

    void executeCommand(const std::string& command);
    std::string getOutput();
    void clearOutput();

private:
    std::vector<std::string> commandHistory;
    std::string currentOutput;
};

#endif // SHELL_H