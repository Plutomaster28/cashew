#pragma once

#include "cashew/common.hpp"
#include <string>
#include <optional>

namespace cashew::crypto {

/**
 * Ed25519 digital signature wrapper
 * Provides signing and verification using libsodium
 */
class Ed25519 {
public:
    /**
     * Generate a new Ed25519 keypair
     * @return pair of (public_key, secret_key)
     */
    static std::pair<PublicKey, SecretKey> generate_keypair();
    
    /**
     * Sign a message with a secret key
     * @param message The message to sign
     * @param secret_key The secret key to sign with
     * @return The signature
     */
    static Signature sign(const bytes& message, const SecretKey& secret_key);
    
    /**
     * Verify a signature
     * @param message The original message
     * @param signature The signature to verify
     * @param public_key The public key to verify against
     * @return true if signature is valid
     */
    static bool verify(const bytes& message, const Signature& signature, const PublicKey& public_key);
    
    /**
     * Derive public key from secret key
     * @param secret_key The secret key
     * @return The corresponding public key
     */
    static PublicKey secret_to_public(const SecretKey& secret_key);
    
    /**
     * Convert public key to hex string
     */
    static std::string public_key_to_hex(const PublicKey& key);
    
    /**
     * Parse public key from hex string
     */
    static std::optional<PublicKey> public_key_from_hex(const std::string& hex);
    
    /**
     * Convert signature to hex string
     */
    static std::string signature_to_hex(const Signature& sig);
    
    /**
     * Parse signature from hex string
     */
    static std::optional<Signature> signature_from_hex(const std::string& hex);
};

} // namespace cashew::crypto
