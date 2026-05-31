#include "base64.h"
#include <stdexcept>
#include <vector>
#include <cctype>   
#include "wrappers.h"

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrst   uvwxyz"
    "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_decode_to_string(const std::string &encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }

    return ret;
}

std::string clean_base64(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c != '\n' && c != '\r' && c != ' ') output += c;
    }
    return output;
}

std::string base64_encode_clean(const unsigned char* data, size_t len, bool url_safe) {
    std::string ret;
    unsigned char arr3[3];
    unsigned char arr4[4];
    int i = 0;

    while (len--) {
        arr3[i++] = *(data++);
        if (i == 3) {
            arr4[0] = (arr3[0] & 0xfc) >> 2;
            arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
            arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
            arr4[3] = arr3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[arr4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            arr3[j] = '\0';

        arr4[0] = (arr3[0] & 0xfc) >> 2;
        arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
        arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
        arr4[3] = arr3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[arr4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    if (url_safe) {
        for (char& c : ret) {
            if (c == '+') c = '-';
            else if (c == '/') c = '_';
        }
    }

    ret = clean_base64(ret);

    return ret;
}



std::string base64_encode(const unsigned char* data, size_t len, bool url_safe) {
    std::string ret;
    unsigned char arr3[3];
    unsigned char arr4[4];
    int i = 0;

    while (len--) {
        arr3[i++] = *(data++);
        if (i == 3) {
            arr4[0] = (arr3[0] & 0xfc) >> 2;
            arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
            arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
            arr4[3] = arr3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[arr4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            arr3[j] = '\0';

        arr4[0] = (arr3[0] & 0xfc) >> 2;
        arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
        arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
        arr4[3] = arr3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[arr4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    if (url_safe) {
        for (char& c : ret) {
            if (c == '+') c = '-';
            else if (c == '/') c = '_';
        }
    }



    return ret;
}

std::string base64_encode(const std::vector<unsigned char>& data) {
    return base64_encode(data.data(), data.size(), false); // explicitly pass false
}


std::vector<unsigned char> base64_decode_bytes(const std::string& input) {
    int in_len = input.size();
    int i = 0;
    int in_ = 0;
    unsigned char arr4[4], arr3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (input[in_] != '=') && is_base64(input[in_])) {
        arr4[i++] = input[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                arr4[i] = base64_chars.find(arr4[i]);

            arr3[0] = (arr4[0] << 2) + ((arr4[1] & 0x30) >> 4);
            arr3[1] = ((arr4[1] & 0xf) << 4) + ((arr4[2] & 0x3c) >> 2);
            arr3[2] = ((arr4[2] & 0x3) << 6) + arr4[3];

            for (i = 0; i < 3; i++)
                ret.push_back(arr3[i]);
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j <4; j++)
            arr4[j] = 0;

        for (int j = 0; j <4; j++)
            arr4[j] = base64_chars.find(arr4[j]);

        arr3[0] = (arr4[0] << 2) + ((arr4[1] & 0x30) >> 4);
        arr3[1] = ((arr4[1] & 0xf) << 4) + ((arr4[2] & 0x3c) >> 2);
        arr3[2] = ((arr4[2] & 0x3) << 6) + arr4[3];

        for (int j = 0; j < i - 1; j++) ret.push_back(arr3[j]);
    }

    return ret;
}
