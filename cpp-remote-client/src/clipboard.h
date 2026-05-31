#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <string>
#include <vector>

class Clipboard {
public:
    // Synchronizes the clipboard content from the remote machine to the local machine
    static bool syncClipboardToLocal(const std::string& clipboardData);

    // Synchronizes the clipboard content from the local machine to the remote machine
    static bool syncClipboardToRemote(const std::string& clipboardData);

    // Retrieves the current clipboard content from the local machine
    static std::string getLocalClipboard();

    // Retrieves the current clipboard content from the remote machine
    static std::string getRemoteClipboard();
};

// Convenience C-style API used by other modules in the project
// (implemented in clipboard.cpp)
bool set_clipboard_text(const std::string& text);
std::string get_clipboard_text();
void sync_clipboard(const std::string& remoteText);

#endif // CLIPBOARD_H