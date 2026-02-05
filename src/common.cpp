#include "cashew/common.hpp"
#include "crypto/blake3.hpp"
#include <sstream>
#include <iomanip>

namespace cashew {

namespace {
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

} // namespace cashew
