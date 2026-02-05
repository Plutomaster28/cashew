#include "blake3.hpp"
#include <blake3.h>
#include <sstream>
#include <iomanip>

namespace cashew::crypto {

namespace {
    std::string bytes_to_hex(const byte* data, size_t len) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            oss << std::setw(2) << static_cast<int>(data[i]);
        }
        return oss.str();
    }
    
    bool hex_to_bytes(const std::string& hex, byte* out, size_t out_len) {
        if (hex.length() != out_len * 2) return false;
        
        for (size_t i = 0; i < out_len; ++i) {
            std::string byte_str = hex.substr(i * 2, 2);
            try {
                out[i] = static_cast<byte>(std::stoul(byte_str, nullptr, 16));
            } catch (...) {
                return false;
            }
        }
        return true;
    }
}

Hash256 Blake3::hash(const bytes& data) {
    Hash256 result;
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());
    blake3_hasher_finalize(&hasher, result.data(), result.size());
    return result;
}

Hash256 Blake3::hash(const std::string& str) {
    bytes data(str.begin(), str.end());
    return hash(data);
}

std::string Blake3::hash_to_hex(const Hash256& hash) {
    return bytes_to_hex(hash.data(), hash.size());
}

std::optional<Hash256> Blake3::hash_from_hex(const std::string& hex) {
    Hash256 hash;
    if (!hex_to_bytes(hex, hash.data(), hash.size())) {
        return std::nullopt;
    }
    return hash;
}

} // namespace cashew::crypto
