#include "power_control.h"
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

bool PowerControl::restart() {
#ifdef _WIN32
    system("shutdown -r -t 0");
    return true;
#else
    system("reboot");
    return true;
#endif
}

bool PowerControl::shutdown() {
#ifdef _WIN32
    system("shutdown -s -t 0");
    return true;
#else
    system("shutdown now");
    return true;
#endif
}

bool PowerControl::logOff() {
#ifdef _WIN32
    system("shutdown -l");
    return true;
#else
    system("pkill -KILL -u $(whoami)");
    return true;
#endif
}

bool PowerControl::sleep() {
    // Not implemented; best-effort fallback
    return false;
}