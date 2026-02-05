#include "ed25519.hpp"
#include <sodium.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace cashew::crypto {

namespace {
    // Ensure libsodium is initialized
    struct SodiumInitializer {
        SodiumInitializer() {
            if (sodium_init() < 0) {
                throw std::runtime_error("Failed to initialize libsodium");
            }
        }
    };
    static SodiumInitializer sodium_init_instance;
    
    // Helper: convert bytes to hex
    std::string bytes_to_hex(const byte* data, size_t len) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            oss << std::setw(2) << static_cast<int>(data[i]);
        }
        return oss.str();
    }
    
    // Helper: convert hex to bytes
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

std::pair<PublicKey, SecretKey> Ed25519::generate_keypair() {
    PublicKey pk;
    SecretKey sk;
    
    if (crypto_sign_keypair(pk.data(), sk.data()) != 0) {
        throw std::runtime_error("Failed to generate Ed25519 keypair");
    }
    
    return {pk, sk};
}

Signature Ed25519::sign(const bytes& message, const SecretKey& secret_key) {
    Signature sig;
    unsigned long long sig_len;
    
    if (crypto_sign_detached(
        sig.data(), 
        &sig_len,
        message.data(),
        message.size(),
        secret_key.data()
    ) != 0) {
        throw std::runtime_error("Failed to sign message");
    }
    
    return sig;
}

bool Ed25519::verify(const bytes& message, const Signature& signature, const PublicKey& public_key) {
    return crypto_sign_verify_detached(
        signature.data(),
        message.data(),
        message.size(),
        public_key.data()
    ) == 0;
}

PublicKey Ed25519::secret_to_public(const SecretKey& secret_key) {
    PublicKey pk;
    if (crypto_sign_ed25519_sk_to_pk(pk.data(), secret_key.data()) != 0) {
        throw std::runtime_error("Failed to derive public key from secret key");
    }
    return pk;
}

std::string Ed25519::public_key_to_hex(const PublicKey& key) {
    return bytes_to_hex(key.data(), key.size());
}

std::optional<PublicKey> Ed25519::public_key_from_hex(const std::string& hex) {
    PublicKey key;
    if (!hex_to_bytes(hex, key.data(), key.size())) {
        return std::nullopt;
    }
    return key;
}

std::string Ed25519::signature_to_hex(const Signature& sig) {
    return bytes_to_hex(sig.data(), sig.size());
}

std::optional<Signature> Ed25519::signature_from_hex(const std::string& hex) {
    Signature sig;
    if (!hex_to_bytes(hex, sig.data(), sig.size())) {
        return std::nullopt;
    }
    return sig;
}

} // namespace cashew::crypto
