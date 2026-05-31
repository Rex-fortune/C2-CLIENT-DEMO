//
//  base64 encoding and decoding with C++.
//  Version: 2.rc.09 (release candidate)
//

#ifndef BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A
#define BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A
#endif

#include <string>   
#include <vector>


std::string base64_encode(const std::vector<unsigned char>& data) ;

std::vector<unsigned char> base64_decode_bytes(const std::string& input) ;

    

std::string base64_encode(unsigned char const*, size_t len, bool url = false);

std::string base64_encode_clean(const unsigned char* data, size_t len, bool url_safe);

std::string base64_decode_to_string(const std::string& b64);



