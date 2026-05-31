#include "sockets.h"

#if defined(_WIN32)
#include <winsock2.h>

bool init_sockets() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void cleanup_sockets() {
    WSACleanup();
}

#else

bool init_sockets() { return true; }

void cleanup_sockets() { }

#endif
