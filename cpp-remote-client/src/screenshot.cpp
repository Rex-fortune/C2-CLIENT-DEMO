#include "screenshot.h"
#include "wrappers.h"
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <gdiplus.h>

void capture_screenshot(std::vector<BYTE>& outBuffer) {
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiStartupInput;
    ULONG_PTR gdiToken;
    Gdiplus::GdiplusStartup(&gdiToken, &gdiStartupInput, NULL);

    // Capture the screenshot
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, screenX, screenY);
    HGDIOBJ oldObj = SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, screenX, screenY, hScreen, 0, 0, SRCCOPY | CAPTUREBLT);
    SelectObject(hDC, oldObj);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    // Save the bitmap to PNG format
    save_bitmap_to_png(hBitmap, outBuffer);
    DeleteObject(hBitmap);

    // Cleanup GDI+
    Gdiplus::GdiplusShutdown(gdiToken);
}

#else // Linux/macOS
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <png.h>

void capture_screenshot(std::vector<unsigned char>& outBuffer) {
    Display* display = XOpenDisplay(NULL);
    if (!display) return;

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    XWindowAttributes gwa;
    XGetWindowAttributes(display, root, &gwa);
    XImage* img = XGetImage(display, root, 0, 0, gwa.width, gwa.height, AllPlanes, ZPixmap);
    if (!img) {
        XCloseDisplay(display);
        return;
    }

    // PNG encoding to memory buffer with libpng
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    struct PngBuffer {
        std::vector<unsigned char>* data;
    } pngBuffer;
    pngBuffer.data = &outBuffer;

    auto png_write_callback = [](png_structp png_ptr, png_bytep data, png_size_t length) {
        PngBuffer* p = (PngBuffer*)png_get_io_ptr(png_ptr);
        p->data->insert(p->data->end(), data, data + length);
    };

    png_set_write_fn(png_ptr, &pngBuffer, png_write_callback, NULL);
    png_set_IHDR(png_ptr, info_ptr, img->width, img->height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);

    std::vector<png_byte> row(img->width * 3);
    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            unsigned long pixel = XGetPixel(img, x, y);
            row[x * 3] = (pixel & img->red_mask) >> 16;
            row[x * 3 + 1] = (pixel & img->green_mask) >> 8;
            row[x * 3 + 2] = (pixel & img->blue_mask);
        }
        png_write_row(png_ptr, row.data());
    }

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    XDestroyImage(img);
    XCloseDisplay(display);
}
#endif

bool capture_screenshot_png(std::vector<unsigned char>& outBuffer) {
#ifdef _WIN32
    std::vector<BYTE> data;
    capture_screenshot(data);
    outBuffer.assign(data.begin(), data.end());
    return !outBuffer.empty();
#else
    std::vector<unsigned char> data;
    capture_screenshot(data);
    outBuffer = std::move(data);
    return !outBuffer.empty();
#endif
}

#if defined(_WIN32)
void save_bitmap_to_png(HBITMAP hBitmap, std::vector<unsigned char>& outBuffer) {
    // Minimal stub: real implementation should encode HBITMAP to PNG bytes
    (void)hBitmap; (void)outBuffer;
}
#endif

void save_screenshot(const std::string& filename) {
    std::vector<unsigned char> screenshotData;
    capture_screenshot_png(screenshotData);
    // Optionally write to file
}