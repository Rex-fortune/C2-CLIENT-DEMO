#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>

// Function to browse remote files and folders
void browse_remote_files(const std::string& path);

// Function to upload a file to the remote machine
bool upload_file(const std::string& localPath, const std::string& remotePath);

// Function to download a file from the remote machine
bool download_file(const std::string& remotePath, const std::string& localPath);

// Function to delete a file on the remote machine
bool delete_file(const std::string& remotePath);

// Function to rename a file on the remote machine
bool rename_file(const std::string& oldName, const std::string& newName);

// Function to move a file on the remote machine
bool move_file(const std::string& sourcePath, const std::string& destinationPath);

// Function to create a new directory on the remote machine
bool create_directory(const std::string& path);

// Function to list files in a directory on the remote machine
std::vector<std::string> list_files(const std::string& path);

#endif // FILE_MANAGER_H