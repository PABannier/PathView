#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace pathview {
namespace util {

inline std::string Base64Encode(const std::vector<uint8_t>& data) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string base64;
    base64.reserve(((data.size() + 2) / 3) * 4);

    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    for (uint8_t byte : data) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (int j = 0; j < 4; j++) {
                base64 += base64_chars[char_array_4[j]];
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int j = 0; j < i + 1; j++) {
            base64 += base64_chars[char_array_4[j]];
        }

        while (i++ < 3) {
            base64 += '=';
        }
    }

    return base64;
}

inline std::vector<uint8_t> Base64Decode(const std::string& input) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    auto is_base64 = [](unsigned char c) {
        return std::isalnum(c) || c == '+' || c == '/';
    };

    std::vector<uint8_t> output;
    int in_len = static_cast<int>(input.size());
    int i = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];

    while (in_len-- && (input[in_] != '=') &&
           is_base64(static_cast<unsigned char>(input[in_]))) {
        char_array_4[i++] = input[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = static_cast<unsigned char>(
                    base64_chars.find(char_array_4[i])
                );
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++) {
                output.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = 0; j < i; j++) {
            char_array_4[j] = static_cast<unsigned char>(
                base64_chars.find(char_array_4[j])
            );
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (int j = 0; j < i - 1; j++) {
            output.push_back(char_array_3[j]);
        }
    }

    return output;
}

}  // namespace util
}  // namespace pathview
