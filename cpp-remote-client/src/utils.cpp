#include <cstddef>  // for size_t
#include <cstring>  // for memset
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>

#include <algorithm>

#include <cctype>
#include "base64.h"


#include <cctype>


#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET sock_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
typedef int sock_t;
#endif



// Send all bytes in buffer
bool send_all(sock_t sock, const char* buffer, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        int sent = send(sock, buffer + total_sent, static_cast<int>(length - total_sent), 0);
        if (sent <= 0) {
            return false; // error or connection closed
        }
        total_sent += sent;
    }
    return true;
}

// Receive exactly length bytes into buffer
bool recv_all(sock_t sock, char* buffer, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        int recvd = recv(sock, buffer + total_received, static_cast<int>(length - total_received), 0);
        if (recvd <= 0) {
            printf("recv_all failed or connection closed\n");
            return false; // error or connection closed
        }
        total_received += recvd;
    }
    return true;
}


// Utility function to split a string by a delimiter
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }

    return tokens;
}

// Utility function to trim whitespace from both ends of a string
std::string trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }

    if (start == str.size()) return ""; // string is all spaces

    size_t end = str.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        end--;
    }

    return str.substr(start, end - start + 1);
}

// Utility function to check if a file exists
bool file_exists(const std::string& filename) {
    std::ifstream f(filename);
    return f.good();
}

// Utility function to read a file into a string
std::string read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return "";

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Utility function to write a string to a file
bool write_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    file.write(content.c_str(), content.size());
    return file.good();
}

