#include <string>
#include "wrappers.h"      
#include "json.hpp"
using json = nlohmann::json;

void handle_file_transfer(sock_t sock,const json& payload,  const std::string& client);