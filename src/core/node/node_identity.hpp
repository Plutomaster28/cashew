#pragma once

#include "cashew/common.hpp"
#include <string>
#include <filesystem>
#include <optional>
#include <vector>

namespace cashew::core {

/**
 * Rotation Certificate - proves that a new key is authorized by the old key
 */
struct RotationCertificate {
    PublicKey old_public_key;      // Previous public key
    PublicKey new_public_key;      // New public key
    uint64_t rotation_timestamp;   // When rotation occurred
    Signature old_key_signature;   // Signature by old key over (new_key || timestamp)
    std::string reason;             // Optional reason for rotation
    
    // Serialize to bytes for signing/verification
    bytes to_bytes() const;
    
    // Verify the certificate
    bool verify() const;
};

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
     * Get rotation certificates (history of key rotations)
     */
    const std::vector<RotationCertificate>& rotation_history() const { return rotation_history_; }
    
    /**
     * Rotate to a new keypair (signs rotation certificate)
     * @param reason Optional reason for rotation (e.g., "Suspected compromise", "Scheduled rotation")
     * @return New NodeIdentity with rotation certificate linking to this one
     */
    NodeIdentity rotate(const std::string& reason = "") const;
    
    /**
     * Verify this identity's rotation chain back to the original key
     * @return true if all rotation certificates are valid
     */
    bool verify_rotation_chain() const;
    
    /**
     * Get the original (genesis) public key from rotation chain
     * @return The first public key in the chain, or current key if no rotations
     */
    PublicKey get_genesis_key() const;
    
    /**
     * Add a rotation certificate to history (used during loading)
     */
    void add_rotation_certificate(const RotationCertificate& cert);
    
private:
    NodeIdentity(const PublicKey& pk, const SecretKey& sk, uint64_t timestamp);
    NodeIdentity(const PublicKey& pk, const SecretKey& sk, uint64_t timestamp, 
                 const std::vector<RotationCertificate>& history);
    
    PublicKey public_key_;
    SecretKey secret_key_;
    NodeID node_id_;
    uint64_t created_timestamp_;
    std::vector<RotationCertificate> rotation_history_;  // Chain of rotation certificates
};

} // namespace cashew::core
