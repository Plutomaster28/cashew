#pragma once

#include "cashew/common.hpp"
#include <optional>

namespace cashew::crypto {

/**
 * X25519 Key Exchange (ECDH)
 * Used for establishing shared secrets between nodes
 */
class X25519 {
public:
    /**
     * Generate a new X25519 keypair
     */
    static std::pair<PublicKey, SecretKey> generate_keypair();
    
    /**
     * Perform key exchange to derive shared secret
     * @param our_secret Our secret key
     * @param their_public Their public key
     * @return Shared secret (32 bytes) or nullopt on failure
     */
    static std::optional<SessionKey> exchange(
        const SecretKey& our_secret,
        const PublicKey& their_public
    );
    
    /**
     * Derive a public key from a secret key
     * @param secret Secret key
     * @return Public key
     */
    static PublicKey derive_public(const SecretKey& secret);
};

} // namespace cashew::crypto
