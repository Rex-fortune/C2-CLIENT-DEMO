#include "file_transfer.h"
#include "sockets.h"
#include "wrappers.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <cstring>

bool upload_file(sock_t sock, const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for upload: " << file_path << std::endl;
        return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Send file size
    if (!send_all(sock, reinterpret_cast<const char*>(&file_size), sizeof(file_size))) {
        std::cerr << "Failed to send file size." << std::endl;
        return false;
    }

    // Send file data
    std::vector<char> buffer(4096);
    size_t total_sent = 0;
    while (total_sent < file_size) {
        file.read(buffer.data(), buffer.size());
        std::streamsize bytes_read = file.gcount();
        if (!send_all(sock, buffer.data(), bytes_read)) {
            std::cerr << "Failed to send file data." << std::endl;
            return false;
        }
        total_sent += bytes_read;
    }

    file.close();
    return true;
}

bool download_file(sock_t sock, const std::string& file_path) {
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for download: " << file_path << std::endl;
        return false;
    }

    // Receive file size
    size_t file_size;
    if (!recv_all(sock, reinterpret_cast<char*>(&file_size), sizeof(file_size))) {
        std::cerr << "Failed to receive file size." << std::endl;
        return false;
    }

    // Receive file data
    std::vector<char> buffer(4096);
    size_t total_received = 0;
    while (total_received < file_size) {
        size_t bytes_to_receive = std::min(buffer.size(), file_size - total_received);
        int bytes_received = recv(sock, buffer.data(), bytes_to_receive, 0);
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive file data." << std::endl;
            return false;
        }
        file.write(buffer.data(), bytes_received);
        total_received += bytes_received;
    }

    file.close();
    return true;
}

bool delete_file(sock_t sock, const std::string& file_path) {
    // Send delete command
    std::string command = "DELETE " + file_path;
    return send_all(sock, command.c_str(), command.size());
}

bool rename_file(sock_t sock, const std::string& old_name, const std::string& new_name) {
    // Send rename command
    std::string command = "RENAME " + old_name + " " + new_name;
    return send_all(sock, command.c_str(), command.size());
}

bool move_file(sock_t sock, const std::string& file_path, const std::string& new_location) {
    // Send move command
    std::string command = "MOVE " + file_path + " " + new_location;
    return send_all(sock, command.c_str(), command.size());
}