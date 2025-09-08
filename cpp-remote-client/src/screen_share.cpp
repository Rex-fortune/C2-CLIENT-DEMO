#include "screen_share.h"
#include "sockets.h"
#include "wrappers.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

void capture_and_send(sock_t sock) {
    std::vector<BYTE> screenshotData;
    while (true) {
        if (capture_screenshot_png(screenshotData)) {
            send_all(sock, reinterpret_cast<const char*>(screenshotData.data()), screenshotData.size());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Adjust the interval as needed
    }
}

#else // Linux/macOS
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <png.h>

void capture_and_send(sock_t sock) {
    std::vector<unsigned char> screenshotData;
    while (true) {
        if (capture_screenshot_png(screenshotData)) {
            send_all(sock, reinterpret_cast<const char*>(screenshotData.data()), screenshotData.size());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Adjust the interval as needed
    }
}
#endif

void start_screen_sharing(sock_t sock) {
    std::thread captureThread(capture_and_send, sock);
    captureThread.detach();
}