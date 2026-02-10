#pragma once

#include "cashew/common.hpp"
#include "core/node/node_identity.hpp"
#include <set>
#include <map>
#include <vector>
#include <optional>
#include <chrono>

namespace cashew::security {

/**
 * KeyRevocationReason - Why a key was revoked
 */
enum class KeyRevocationReason {
    SUSPECTED_COMPROMISE,    // Key may have been stolen
    CONFIRMED_COMPROMISE,    // Key definitely compromised
    SCHEDULED_ROTATION,      // Regular rotation schedule
    DEVICE_LOSS,             // Device lost or stolen
    KEY_EXPIRATION,          // Key reached end of life
    POLICY_VIOLATION,        // Key used inappropriately
    ADMINISTRATIVE,          // Admin-initiated revocation
    OWNER_REQUEST            // Key owner requested revocation
};

/**
 * KeyRevocation - Record of a revoked key
 */
struct KeyRevocation {
    PublicKey revoked_key;           // The key being revoked
    KeyRevocationReason reason;
    uint64_t revoked_at;             // Timestamp of revocation
    NodeID revoker;                  // Who revoked it
    std::optional<PublicKey> replacement_key;  // New key (if rotated)
    std::optional<core::RotationCertificate> rotation_cert;  // Proof of rotation
    Signature signature;             // Signature by revoker or replacement key
    
    /**
     * Serialize for signing or transmission
     */
    bytes to_bytes() const;
    static std::optional<KeyRevocation> from_bytes(const bytes& data);
    
    /**
     * Generate unique ID for deduplication
     */
    Hash256 get_id() const;
};

/**
 * KeyRevocationList - Batch of revocations for gossip
 */
struct KeyRevocationList {
    std::vector<KeyRevocation> revocations;
    uint64_t timestamp;
    NodeID source_node;
    Signature signature;
    
    bytes to_bytes() const;
    static std::optional<KeyRevocationList> from_bytes(const bytes& data);
};

/**
 * KeyRevocationBroadcaster - Manages key revocation and gossip propagation
 * 
 * Design:
 * 1. Local revocation - Node can revoke its own keys
 * 2. Network-wide gossip - Revocations propagated to all peers
 * 3. Revocation checking - Fast lookup for revoked keys
 * 4. Rotation support - Revocation + new key in one message
 * 5. Deduplication - Track seen revocations
 * 
 * Integration with rotation certificates:
 * - When a key is rotated, create revocation for old key
 * - Include rotation certificate as proof
 * - Broadcast both revocation and new key
 */
class KeyRevocationBroadcaster {
public:
    KeyRevocationBroadcaster();
    ~KeyRevocationBroadcaster() = default;
    
    /**
     * Revoke a key
     * @param revoked_key The key to revoke
     * @param reason Why it's being revoked
     * @param revoker Who is revoking (must be key owner or admin)
     * @param replacement_key Optional new key (for rotation)
     * @param rotation_cert Optional rotation certificate
     * @return The revocation record if successful
     */
    std::optional<KeyRevocation> revoke_key(
        const PublicKey& revoked_key,
        KeyRevocationReason reason,
        const NodeID& revoker,
        const std::optional<PublicKey>& replacement_key = std::nullopt,
        const std::optional<core::RotationCertificate>& rotation_cert = std::nullopt
    );
    
    /**
     * Check if a key has been revoked
     * @param public_key The key to check
     * @return true if key is revoked
     */
    bool is_key_revoked(const PublicKey& public_key) const;
    
    /**
     * Get revocation record for a key
     * @param public_key The key to look up
     * @return Revocation record if found
     */
    std::optional<KeyRevocation> get_revocation(const PublicKey& public_key) const;
    
    /**
     * Get replacement key for a revoked key
     * @param revoked_key The old (revoked) key
     * @return The new key if revocation included one
     */
    std::optional<PublicKey> get_replacement_key(const PublicKey& revoked_key) const;
    
    /**
     * Process a revocation from another node (via gossip)
     * @param revocation The revocation record
     * @param source_node Which node sent it
     * @return true if revocation was accepted
     */
    bool process_revocation(const KeyRevocation& revocation, const NodeID& source_node);
    
    /**
     * Process a batch revocation list
     * @param list The batch of revocations
     * @return Number of new revocations accepted
     */
    size_t process_revocation_list(const KeyRevocationList& list);
    
    /**
     * Create a revocation list for gossip
     * @param include_all If true, include all revocations; if false, only recent
     * @return Revocation list ready for signing and transmission
     */
    KeyRevocationList create_revocation_list(bool include_all = false) const;
    
    /**
     * Get recent revocations (for gossip)
     * @param since_timestamp Only revocations after this time
     * @param max_count Maximum number to return
     * @return Vector of recent revocations
     */
    std::vector<KeyRevocation> get_recent_revocations(
        uint64_t since_timestamp = 0,
        size_t max_count = 100
    ) const;
    
    /**
     * Verify a revocation signature
     * @param revocation The revocation to verify
     * @return true if signature is valid
     */
    static bool verify_revocation(const KeyRevocation& revocation);
    
    /**
     * Sign a revocation
     * @param revocation The revocation to sign (modified in place)
     * @param signature The signature to apply
     */
    static void sign_revocation(KeyRevocation& revocation, const Signature& signature);
    
    // Statistics and maintenance
    size_t revocation_count() const;
    size_t expired_revocation_count() const;
    void cleanup_expired_revocations();
    
    // Configuration
    void set_revocation_expiry_days(uint32_t days) { revocation_expiry_days_ = days; }
    
private:
    // Revocation storage
    std::map<PublicKey, KeyRevocation> revocations_;  // By revoked key
    std::map<PublicKey, PublicKey> key_replacements_; // Old key -> new key
    
    // Seen revocations (for deduplication)
    std::set<Hash256> seen_revocations_;
    
    // Configuration
    uint32_t revocation_expiry_days_;
    uint32_t max_propagation_count_;
    
    // Helpers
    bool is_revocation_expired(const KeyRevocation& revocation) const;
    bool should_accept_revocation(const KeyRevocation& revocation) const;
    uint64_t current_timestamp() const;
};

/**
 * Helper to convert KeyRevocationReason to string
 */
const char* key_revocation_reason_to_string(KeyRevocationReason reason);

} // namespace cashew::security
