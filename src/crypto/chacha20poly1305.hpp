#pragma once

#include "cashew/common.hpp"

namespace cashew::crypto {

/**
 * ChaCha20-Poly1305 authenticated encryption wrapper
 */
class ChaCha20Poly1305 {
public:
    /**
     * Encrypt data with ChaCha20-Poly1305
     * @param plaintext The data to encrypt
     * @param key The 32-byte encryption key
     * @param nonce The 12-byte nonce (must be unique per message)
     * @return Ciphertext with 16-byte authentication tag appended
     */
    static bytes encrypt(const bytes& plaintext, const SessionKey& key, const Nonce& nonce);
    
    /**
     * Decrypt data with ChaCha20-Poly1305
     * @param ciphertext The encrypted data (includes 16-byte auth tag)
     * @param key The 32-byte encryption key
     * @param nonce The 12-byte nonce used for encryption
     * @return Decrypted plaintext, or nullopt if authentication fails
     */
    static std::optional<bytes> decrypt(const bytes& ciphertext, const SessionKey& key, const Nonce& nonce);
    
    /**
     * Generate a random session key
     */
    static SessionKey generate_key();
    
    /**
     * Generate a random nonce
     */
    static Nonce generate_nonce();
};

} // namespace cashew::crypto
