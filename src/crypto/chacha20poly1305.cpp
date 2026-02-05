#include "chacha20poly1305.hpp"
#include <sodium.h>
#include <stdexcept>

namespace cashew::crypto {

bytes ChaCha20Poly1305::encrypt(const bytes& plaintext, const SessionKey& key, const Nonce& nonce) {
    bytes ciphertext(plaintext.size() + crypto_aead_chacha20poly1305_ietf_ABYTES);
    unsigned long long ciphertext_len;
    
    if (crypto_aead_chacha20poly1305_ietf_encrypt(
        ciphertext.data(),
        &ciphertext_len,
        plaintext.data(),
        plaintext.size(),
        nullptr, // no additional data
        0,
        nullptr, // not used
        nonce.data(),
        key.data()
    ) != 0) {
        throw std::runtime_error("ChaCha20-Poly1305 encryption failed");
    }
    
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::optional<bytes> ChaCha20Poly1305::decrypt(const bytes& ciphertext, const SessionKey& key, const Nonce& nonce) {
    if (ciphertext.size() < crypto_aead_chacha20poly1305_ietf_ABYTES) {
        return std::nullopt; // Ciphertext too short
    }
    
    bytes plaintext(ciphertext.size() - crypto_aead_chacha20poly1305_ietf_ABYTES);
    unsigned long long plaintext_len;
    
    if (crypto_aead_chacha20poly1305_ietf_decrypt(
        plaintext.data(),
        &plaintext_len,
        nullptr, // not used
        ciphertext.data(),
        ciphertext.size(),
        nullptr, // no additional data
        0,
        nonce.data(),
        key.data()
    ) != 0) {
        return std::nullopt; // Authentication failed
    }
    
    plaintext.resize(plaintext_len);
    return plaintext;
}

SessionKey ChaCha20Poly1305::generate_key() {
    SessionKey key;
    randombytes_buf(key.data(), key.size());
    return key;
}

Nonce ChaCha20Poly1305::generate_nonce() {
    Nonce nonce;
    randombytes_buf(nonce.data(), nonce.size());
    return nonce;
}

} // namespace cashew::crypto
