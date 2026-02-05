#include "argon2.hpp"
#include <sodium.h>
#include <stdexcept>
#include <cstring>

namespace cashew::crypto {

bytes Argon2::hash(
    const bytes& password,
    const bytes& salt,
    const Params& params,
    size_t output_len
) {
    if (salt.size() < crypto_pwhash_SALTBYTES) {
        throw std::invalid_argument("Salt too short (minimum 16 bytes)");
    }
    
    bytes hash(output_len);
    
    int result = crypto_pwhash(
        hash.data(),
        hash.size(),
        reinterpret_cast<const char*>(password.data()),
        password.size(),
        salt.data(),
        params.time_cost,
        params.memory_cost_kb * 1024,  // Convert KB to bytes
        crypto_pwhash_ALG_ARGON2ID13
    );
    
    if (result != 0) {
        throw std::runtime_error("Argon2 hashing failed (out of memory?)");
    }
    
    return hash;
}

bool Argon2::verify(
    const bytes& password,
    const bytes& salt,
    const bytes& expected_hash,
    const Params& params
) {
    try {
        bytes computed_hash = hash(password, salt, params, expected_hash.size());
        return computed_hash == expected_hash;
    } catch (...) {
        return false;
    }
}

Hash256 Argon2::solve_puzzle(
    const bytes& challenge,
    uint64_t nonce,
    const Params& params
) {
    // Combine challenge and nonce
    bytes input = challenge;
    input.reserve(input.size() + sizeof(nonce));
    for (size_t i = 0; i < sizeof(nonce); ++i) {
        input.push_back(static_cast<byte>((nonce >> (i * 8)) & 0xFF));
    }
    
    // Generate salt from challenge (deterministic)
    bytes salt(crypto_pwhash_SALTBYTES);
    crypto_generichash(salt.data(), salt.size(),
                      challenge.data(), challenge.size(),
                      nullptr, 0);
    
    // Hash with Argon2
    bytes hash_result = hash(input, salt, params, 32);
    
    // Convert to Hash256
    Hash256 result;
    std::memcpy(result.data(), hash_result.data(), 32);
    return result;
}

} // namespace cashew::crypto
