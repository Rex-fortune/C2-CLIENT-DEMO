#include "clipboard.h"
#include <string>
#include <vector>
#include <windows.h> // For Windows clipboard functions

// Function to set text to the clipboard
bool set_clipboard_text(const std::string& text) {
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hGlob) {
            memcpy(GlobalLock(hGlob), text.c_str(), text.size() + 1);
            GlobalUnlock(hGlob);
            SetClipboardData(CF_TEXT, hGlob);
        }
        CloseClipboard();
        return true;
    }
    return false;
}

// Function to get text from the clipboard
std::string get_clipboard_text() {
    std::string result;
    if (OpenClipboard(NULL)) {
        HGLOBAL hGlob = GetClipboardData(CF_TEXT);
        if (hGlob) {
            char* pData = static_cast<char*>(GlobalLock(hGlob));
            if (pData) {
                result = pData;
                GlobalUnlock(hGlob);
            }
        }
        CloseClipboard();
    }
    return result;
}

// Function to synchronize clipboard text between local and remote
void sync_clipboard(const std::string& remoteText) {
    set_clipboard_text(remoteText);
}