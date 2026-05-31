#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <codecvt>     // for std::wstring_convert
#include <locale>      // for codecvt

#ifdef _WIN32
    #include <windows.h>
    #include <lm.h>        // NetUserEnum, NetUserAdd, NetUserSetInfo, NetUserDel
    #pragma comment(lib, "netapi32.lib")
#else
    #include <pwd.h>       // getpwent, passwd struct
    #include <unistd.h>    // system, etc.
#endif

// Project headers (adjust paths as needed)
#include "utils.h"         // split, send_result, exec_command
#include "wrappers.h"      // sock_t type



void handle_security(sock_t sock, const json& payload, const std::string& client) {
    printf("Handling security command...\n");
    std::ostringstream result;
    const std::string cmd = payload.value("cmd", "");
    const std::string username = payload.value("username", "");
    const std::string password = payload.value("password", "");
    const std::string group = payload.value("group", "");
    const std::string user = payload.value("user", "");


 

    // 🔹 User listing
    if (cmd == "USERS") {

       #ifdef _WIN32
        LPUSER_INFO_0 pBuf = NULL;
        DWORD dwEntriesRead = 0, dwTotalEntries = 0, dwResumeHandle = 0;
        NET_API_STATUS nStatus = NetUserEnum(NULL, 0, FILTER_NORMAL_ACCOUNT,
                                             (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH,
                                             &dwEntriesRead, &dwTotalEntries, &dwResumeHandle);
        if ((nStatus == NERR_Success || nStatus == ERROR_MORE_DATA) && pBuf) {
            for (DWORD i = 0; i < dwEntriesRead; i++) {
                result << std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(pBuf[i].usri0_name) << "\n";
            }
        } else {
            result << "Failed to enumerate users\n";
        }
        if (pBuf) NetApiBufferFree(pBuf);
       #else
        struct passwd *pw;
        setpwent();
        while ((pw = getpwent()) != NULL) {
        result << pw->pw_name << "\n";
        }
        endpwent();
       #endif
    }

    // 🔹 Add user
    else if (cmd == "ADD_USER" ) {
      
#ifdef _WIN32
        USER_INFO_1 ui;
        memset(&ui, 0, sizeof(ui));
        std::wstring wuser(username.begin(), username.end());
        std::wstring wpass(password.begin(), password.end());
        ui.usri1_name = (LPWSTR)wuser.c_str();
        ui.usri1_password = (LPWSTR)wpass.c_str();
        ui.usri1_priv = USER_PRIV_USER;
        ui.usri1_flags = UF_SCRIPT;
        NET_API_STATUS nStatus = NetUserAdd(NULL, 1, (LPBYTE)&ui, NULL);
        result << ((nStatus == NERR_Success) ? "User added.\n" : "Failed to add user.\n");
#else
        std::string cmd = "useradd -m " + username + " && echo '" + username + ":" + password + "' | chpasswd";
        int rc = system(cmd.c_str());
        result << ((rc == 0) ? "User added.\n" : "Failed to add user.\n");
#endif
    }

    // 🔹 Disable user
    else if (cmd == "DISABLE_USER" ) {

#ifdef _WIN32
        USER_INFO_1008 ui;
        ui.usri1008_flags = UF_ACCOUNTDISABLE;
        std::wstring wuser(username.begin(), username.end());
        NET_API_STATUS nStatus = NetUserSetInfo(NULL, wuser.c_str(), 1008, (LPBYTE)&ui, NULL);
        result << ((nStatus == NERR_Success) ? "User disabled.\n" : "Failed to disable user.\n");
#else
        std::string cmd = "usermod -L " + username;
        int rc = system(cmd.c_str());
        result << ((rc == 0) ? "User disabled.\n" : "Failed to disable user.\n");
#endif
    }
    // Inside handle_security()

    // 🔹 Reset password
    else if (cmd == "RESET_PASS" ) {
        
#ifdef _WIN32
        USER_INFO_1003 ui;
        std::wstring wpass(password.begin(), password.end());
        ui.usri1003_password = (LPWSTR)wpass.c_str();

        std::wstring wuser(username.begin(), username.end());
        NET_API_STATUS nStatus = NetUserSetInfo(NULL, wuser.c_str(), 1003, (LPBYTE)&ui, NULL);

        result << ((nStatus == NERR_Success) ? "Password reset.\n" : "Failed to reset password.\n");
#else
        std::string cmd = "echo '" + username + ":" + password + "' | chpasswd";
        int rc = system(cmd.c_str());
        result << ((rc == 0) ? "Password reset.\n" : "Failed to reset password.\n");
#endif
    }
// --- GROUP MANAGEMENT ---
    else if (cmd == "LIST_GROUPS") {
#ifdef _WIN32
        result << exec_command("net localgroup");
#else
        result << exec_command("cut -d: -f1 /etc/group");
#endif
    }
    else if (cmd == "ADD_GROUP" ) {

#ifdef _WIN32
        std::string cmd = "net localgroup " + group + " /add";
#else
        std::string cmd = "sudo groupadd " + group;
#endif
        result << (system(cmd.c_str()) == 0 ? "Group created.\n" : "Failed to create group.\n");
    }
    else if (cmd == "DELETE_GROUP" ) {
#ifdef _WIN32
        std::string cmd = "net localgroup " + group + " /delete";
#else
        std::string cmd = "sudo groupdel " + group;
#endif
        result << (system(cmd.c_str()) == 0 ? "Group deleted.\n" : "Failed to delete group.\n");
    }
    else if (cmd == "ADD_USER_TO_GROUP") {

#ifdef _WIN32
        std::string cmd = "net localgroup " + group + " " + user + " /add";
#else
        std::string cmd = "sudo usermod -aG " + group + " " + user;
#endif
        result << (system(cmd.c_str()) == 0 ? "User added to group.\n" : "Failed to add user to group.\n");
    }
    else if (cmd == "REMOVE_USER_FROM_GROUP" ) {
#ifdef _WIN32
        std::string cmd = "net localgroup " + group + " " + user + " /delete";
#else
        std::string cmd = "sudo gpasswd -d " + user + " " + group;
#endif
        result << (system(cmd.c_str()) == 0 ? "User removed from group.\n" : "Failed to remove user from group.\n");
    }

    // 🔹 Enable user
    else if (cmd == "ENABLE_USER" ) {
#ifdef _WIN32
        USER_INFO_1008 ui;
        ui.usri1008_flags = UF_SCRIPT; // Normal enabled account
        std::wstring wuser(username.begin(), username.end());
        NET_API_STATUS nStatus = NetUserSetInfo(NULL, wuser.c_str(), 1008, (LPBYTE)&ui, NULL);
        result << ((nStatus == NERR_Success) ? "User enabled.\n" : "Failed to enable user.\n");
#else
        std::string cmd = "usermod -U " + username;
        int rc = system(cmd.c_str());
        result << ((rc == 0) ? "User enabled.\n" : "Failed to enable user.\n");
#endif
    }

    // 🔹 Delete user
    else if (cmd == "DELETE_USER" ) {

#ifdef _WIN32
        std::wstring wuser(username.begin(), username.end());
        NET_API_STATUS nStatus = NetUserDel(NULL, wuser.c_str());
        result << ((nStatus == NERR_Success) ? "User deleted.\n" : "Failed to delete user.\n");
#else
        std::string cmd = "userdel -r " + username;
        int rc = system(cmd.c_str());
        result << ((rc == 0) ? "User deleted.\n" : "Failed to delete user.\n");
#endif
    }

    else {
        result << "Unknown SECURITY command.\n";
    }
    printf("Result security: %s", result.str().c_str());

    fflush(stdout);

    send_result(sock, payload, client, result.str(), false);
}
