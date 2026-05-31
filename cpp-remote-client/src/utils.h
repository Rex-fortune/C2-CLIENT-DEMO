#pragma once
#ifndef UTILS_H
#define UTILS_H
#include <windows.h>



#include <string>
#include <vector>
#include "sockets.h"


#include "json.hpp"
using json = nlohmann::json;




std::string generate_uuid();

const size_t CHUNK_SIZE = 64 * 1024; // 64KB



void send_result(sock_t sock, const json& payload_, const std::string& client, const std::string& data, bool is_binary = false);

void send_result_safe(sock_t sock, const json& payload, const std::string& client,  const std::vector<BYTE>& frameData);

extern bool send_all(sock_t sock, const char* buffer, size_t length);
extern bool recv_all(sock_t sock, char* buffer, size_t length);

// Utility function to split a string by a delimiter
std::vector<std::string> split(const std::string& str, char delimiter);


// Utility function to trim whitespace from both ends of a string
std::string trim(const std::string& str);

// Utility function to check if a file exists
bool file_exists(const std::string& filename);

// Utility function to read a file into a string
std::string read_file(const std::string& filename);

// Utility function to write a string to a file
bool write_file(const std::string& filename, const std::string& content);

#endif // UTILS_H