#include <string>
#include <vector>
#include <fstream>      // ifstream, ofstream
#include <algorithm>    // std::min
#include <cstdint>      // uint32_t
#include <cstring>      // memcpy, etc.
#ifdef _WIN32
    #include <winsock2.h>   // htonl, sockets
    #include <windows.h>  
    
    // optional, but winsock2 must come first
#endif

// External libs
#include "json.hpp"   // JSON handling

// Project headers
#include "base64.h"       // base64_encode, base64_decode_bytes
     // base64_encode, base64_decode_bytes
#include "wrappers.h"     // sock_t, send_all, recv_all
#include "utils.h"        // send_result, maybe CHUNK_SIZE


void handle_file_transfer(sock_t sock, const json& payload, const std::string& client) {
    std::string cmd    = payload.value("cmd", "");
    std::string local  = payload.value("local", "");
    std::string remote = payload.value("remote", "");
    size_t offset      = payload.value("offset", 0);

    if (cmd == "download") {
        std::ifstream file(remote, std::ios::binary);
        if (!file) {
            send_result(sock, payload, client, "File not found: " + remote, false);
            return;
        }

        file.seekg(0, std::ios::end);
        size_t filesize = file.tellg();
        file.seekg(offset, std::ios::beg);

        std::vector<unsigned char> buffer(std::min(CHUNK_SIZE, filesize - offset));
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        size_t bytesRead = file.gcount();
        buffer.resize(bytesRead);

        json resp = {
            {"status", "ok"},
            {"cmd", "download"},
            {"remote", remote},
            {"local", local},
            {"offset", offset},
            {"filesize", filesize},
            {"data", base64_encode(buffer)}
        };

        send_result(sock, payload, client, resp.dump(), false);
    }

    else if (cmd == "upload") {
        std::string chunkB64 = payload.value("data", "");
        std::vector<unsigned char> chunk = base64_decode_bytes(chunkB64);

        std::ofstream file(remote, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) {
            file.open(remote, std::ios::binary | std::ios::out);
            if (!file) {
                send_result(sock, payload, client, "Failed to write file: " + remote, false);
                return;
            }
        }

        file.seekp(offset, std::ios::beg);
        file.write(reinterpret_cast<char*>(chunk.data()), chunk.size());
        file.close();

        json ack = {
            {"status", "ok"},
            {"cmd", "upload"},
            {"remote", remote},
            {"local", local},
            {"received", offset + chunk.size()}
        };

        send_result(sock, payload, client, ack.dump(), false);
    }

    else if (cmd == "delete") {
        if (std::remove(remote.c_str()) == 0) {
            json resp = {{"status", "ok"}, {"cmd", "delete"}, {"remote", remote}};
            send_result(sock, payload, client, resp.dump(), false);
        } else {
            send_result(sock, payload, client, "Failed to delete: " + remote, false);
        }
    }

    else if (cmd == "rename") {
        std::string oldPath = payload.value("old", "");
        std::string newPath = payload.value("new", "");
        if (std::rename(oldPath.c_str(), newPath.c_str()) == 0) {
            json resp = {{"status", "ok"}, {"cmd", "rename"}, {"old", oldPath}, {"new", newPath}};
            send_result(sock, payload, client, resp.dump(), false);
        } else {
            send_result(sock, payload, client, "Failed to rename file", false);
        }
    }

    else if (cmd == "move") {
        std::string src = payload.value("src", "");
        std::string dst = payload.value("dst", "");
        if (std::rename(src.c_str(), dst.c_str()) == 0) {
            json resp = {{"status", "ok"}, {"cmd", "move"}, {"src", src}, {"dst", dst}};
            send_result(sock, payload, client, resp.dump(), false);
        } else {
            send_result(sock, payload, client, "Failed to move file", false);
        }
    }

    else {
        send_result(sock, payload, client, "Unknown TRANSFER command\n", false);
    }
}
