#pragma once

#include "cashew/common.hpp"
#include <optional>

namespace cashew::crypto {

/**
 * BLAKE3 cryptographic hash function wrapper
 */
class Blake3 {
public:
    /**
     * Hash data using BLAKE3
     * @param data The data to hash
     * @return 32-byte hash
     */
    static Hash256 hash(const bytes& data);
    
    /**
     * Hash a string using BLAKE3
     */
    static Hash256 hash(const std::string& str);
    
    /**
     * Convert hash to hex string
     */
    static std::string hash_to_hex(const Hash256& hash);
    
    /**
     * Parse hash from hex string
     */
    static std::optional<Hash256> hash_from_hex(const std::string& hex);
};

} // namespace cashew::crypto
