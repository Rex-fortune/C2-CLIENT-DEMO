#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock_t;
#define CLOSESOCKET closesocket
#define SOCKET_ERR INVALID_SOCKET
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int sock_t;
#define CLOSESOCKET close
#define SOCKET_ERR -1
#endif

// Basic helpers used by many modules
bool init_sockets();
void cleanup_sockets();
