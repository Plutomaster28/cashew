#include "x25519.hpp"
#include <sodium.h>
#include <stdexcept>

namespace cashew::crypto {

std::pair<PublicKey, SecretKey> X25519::generate_keypair() {
    PublicKey public_key;
    SecretKey secret_key;
    
    if (crypto_box_keypair(public_key.data(), secret_key.data()) != 0) {
        throw std::runtime_error("Failed to generate X25519 keypair");
    }
    
    return {public_key, secret_key};
}

std::optional<SessionKey> X25519::exchange(
    const SecretKey& our_secret,
    const PublicKey& their_public
) {
    SessionKey shared_secret;
    
    if (crypto_scalarmult(shared_secret.data(), 
                         our_secret.data(), 
                         their_public.data()) != 0) {
        return std::nullopt;
    }
    
    return shared_secret;
}

PublicKey X25519::derive_public(const SecretKey& secret) {
    PublicKey public_key;
    
    if (crypto_scalarmult_base(public_key.data(), secret.data()) != 0) {
        throw std::runtime_error("Failed to derive X25519 public key");
    }
    
    return public_key;
}

} // namespace cashew::crypto
