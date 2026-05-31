#include "file_manager.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <sys/stat.h>

namespace fs = std::filesystem;

class FileManager {
public:
    void browseFiles(const std::string& path);
    bool uploadFile(const std::string& localPath, const std::string& remotePath);
    bool downloadFile(const std::string& remotePath, const std::string& localPath);
    bool deleteFile(const std::string& path);
    bool renameFile(const std::string& oldPath, const std::string& newPath);
    bool moveFile(const std::string& sourcePath, const std::string& destinationPath);
};

void FileManager::browseFiles(const std::string& path) {
    for (const auto& entry : fs::directory_iterator(path)) {
        std::cout << entry.path().string() << std::endl;
    }
}

bool FileManager::uploadFile(const std::string& localPath, const std::string& remotePath) {
    std::ifstream src(localPath, std::ios::binary);
    std::ofstream dst(remotePath, std::ios::binary);
    dst << src.rdbuf();
    return src && dst;
}

bool FileManager::downloadFile(const std::string& remotePath, const std::string& localPath) {
    std::ifstream src(remotePath, std::ios::binary);
    std::ofstream dst(localPath, std::ios::binary);
    dst << src.rdbuf();
    return src && dst;
}

bool FileManager::deleteFile(const std::string& path) {
    return fs::remove(path);
}

bool FileManager::renameFile(const std::string& oldPath, const std::string& newPath) {
    fs::rename(oldPath, newPath);
    return fs::exists(newPath);
}

bool FileManager::moveFile(const std::string& sourcePath, const std::string& destinationPath) {
    fs::rename(sourcePath, destinationPath);
    return fs::exists(destinationPath);
}