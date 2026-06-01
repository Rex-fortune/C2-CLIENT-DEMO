#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif

bool capture_screenshot_png(std::vector<unsigned char>& outBuffer);

// Helper for Windows: convert HBITMAP to PNG bytes
#if defined(_WIN32)
void save_bitmap_to_png(HBITMAP hBitmap, std::vector<unsigned char>& outBuffer);
#endif

#endif // SCREENSHOT_H