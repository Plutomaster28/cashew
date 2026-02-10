#include "cashew/common.hpp"
#include "crypto/blake3.hpp"
#include <sstream>
#include <iomanip>

namespace cashew {

// Utility functions
std::string hash_to_hex(const Hash256& hash) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : hash) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

Hash256 hex_to_hash(const std::string& hex) {
    Hash256 hash;
    if (hex.length() != hash.size() * 2) {
        throw std::runtime_error("Invalid hex string length");
    }
    for (size_t i = 0; i < hash.size(); ++i) {
        hash[i] = static_cast<byte>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    return hash;
}

// NodeID implementation
std::string NodeID::to_string() const {
    return hash_to_hex(id);
}

NodeID NodeID::from_string(const std::string& str) {
    return NodeID(hex_to_hash(str));
}

// HumanID implementation
std::string HumanID::to_string() const {
    return hash_to_hex(id);
}

HumanID HumanID::from_string(const std::string& str) {
    return HumanID(hex_to_hash(str));
}

// NetworkID implementation
std::string NetworkID::to_string() const {
    return hash_to_hex(id);
}

NetworkID NetworkID::from_string(const std::string& str) {
    return NetworkID(hex_to_hash(str));
}

// ContentHash implementation
std::string ContentHash::to_string() const {
    return hash_to_hex(hash);
}

ContentHash ContentHash::from_string(const std::string& str) {
    return ContentHash(hex_to_hash(str));
}

// Base64 implementation
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const void* data, size_t len) {
    const byte* bytes_data = static_cast<const byte*>(data);
    std::string result;
    result.reserve(((len + 2) / 3) * 4);
    
    size_t i = 0;
    byte char_array_3[3];
    byte char_array_4[4];
    
    while (len--) {
        char_array_3[i++] = *(bytes_data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) {
                result += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (size_t j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (size_t j = 0; j < i + 1; j++) {
            result += base64_chars[char_array_4[j]];
        }
        
        while (i++ < 3) {
            result += '=';
        }
    }
    
    return result;
}

std::string base64_encode(const bytes& data) {
    return base64_encode(data.data(), data.size());
}

static inline bool is_base64(byte c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

bytes base64_decode(const std::string& encoded) {
    size_t in_len = encoded.size();
    size_t i = 0;
    size_t in_ = 0;
    byte char_array_4[4], char_array_3[3];
    bytes result;
    
    while (in_len-- && (encoded[in_] != '=') && is_base64(encoded[in_])) {
        char_array_4[i++] = encoded[in_]; in_++;
        if (i == 4) {
            for (size_t j = 0; j < 4; j++) {
                char_array_4[j] = static_cast<byte>(std::string(base64_chars).find(char_array_4[j]));
            }
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (size_t j = 0; j < 3; j++) {
                result.push_back(char_array_3[j]);
            }
            i = 0;
        }
    }
    
    if (i) {
        for (size_t j = 0; j < i; j++) {
            char_array_4[j] = static_cast<byte>(std::string(base64_chars).find(char_array_4[j]));
        }
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        
        for (size_t j = 0; j < i - 1; j++) {
            result.push_back(char_array_3[j]);
        }
    }
    
    return result;
}

} // namespace cashew
