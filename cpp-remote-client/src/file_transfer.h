#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <vector>
#include <string>

class FileTransfer {
public:
    FileTransfer(const std::string& serverAddress, int port);
    ~FileTransfer();

    bool uploadFile(const std::string& localPath, const std::string& remotePath);
    bool downloadFile(const std::string& remotePath, const std::string& localPath);
    bool deleteFile(const std::string& remotePath);
    bool renameFile(const std::string& oldRemotePath, const std::string& newRemotePath);
    bool moveFile(const std::string& sourceRemotePath, const std::string& destinationRemotePath);
    std::vector<std::string> listFiles(const std::string& remoteDirectory);
    bool resumeFileTransfer(const std::string& remotePath, const std::string& localPath);
    void setProgressCallback(void (*callback)(size_t, size_t));

private:
    std::string serverAddress;
    int port;
    void (*progressCallback)(size_t, size_t);

    bool sendFile(const std::string& filePath);
    bool receiveFile(const std::string& filePath);
    void updateProgress(size_t bytesTransferred, size_t totalBytes);
};

#endif // FILE_TRANSFER_H