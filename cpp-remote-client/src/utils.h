#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

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