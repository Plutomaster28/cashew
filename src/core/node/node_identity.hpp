#pragma once

#include "cashew/common.hpp"
#include <string>
#include <filesystem>
#include <optional>

namespace cashew::core {

/**
 * Node Identity - cryptographic identity for a Cashew node
 * Based on Ed25519 keypair
 */
class NodeIdentity {
public:
    /**
     * Generate a new random node identity
     */
    static NodeIdentity generate();
    
    /**
     * Load identity from encrypted file
     * @param path Path to identity file
     * @param password Password to decrypt the file (optional if not encrypted)
     */
    static NodeIdentity load(const std::filesystem::path& path, const std::string& password = "");
    
    /**
     * Save identity to encrypted file
     * @param path Path to save to
     * @param password Password to encrypt with (empty = no encryption)
     */
    void save(const std::filesystem::path& path, const std::string& password = "") const;
    
    /**
     * Get the node ID (derived from public key)
     */
    const NodeID& id() const { return node_id_; }
    
    /**
     * Get the public key
     */
    const PublicKey& public_key() const { return public_key_; }
    
    /**
     * Sign a message
     */
    Signature sign(const bytes& message) const;
    
    /**
     * Verify a signature against this identity's public key
     */
    bool verify(const bytes& message, const Signature& signature) const;
    
    /**
     * Get creation timestamp
     */
    uint64_t created_at() const { return created_timestamp_; }
    
    /**
     * Rotate to a new keypair (signs rotation certificate)
     */
    NodeIdentity rotate() const;
    
private:
    NodeIdentity(const PublicKey& pk, const SecretKey& sk, uint64_t timestamp);
    
    PublicKey public_key_;
    SecretKey secret_key_;
    NodeID node_id_;
    uint64_t created_timestamp_;
};

} // namespace cashew::core
