#pragma once

#include "cashew/common.hpp"
#include "security/access.hpp"
#include <set>
#include <map>
#include <vector>
#include <optional>
#include <chrono>

namespace cashew::security {

/**
 * RevocationReason - Why a token was revoked
 */
enum class RevocationReason {
    MANUAL_REVOCATION,      // Explicitly revoked by issuer
    COMPROMISED_KEY,        // Key suspected to be compromised
    POLICY_VIOLATION,       // Node violated access policies
    ABUSE_DETECTED,         // Node engaged in abuse
    EXPIRED_CREDENTIALS,    // Underlying credentials expired
    NETWORK_REMOVAL,        // Node removed from network
    REPUTATION_LOSS         // Reputation dropped below threshold
};

/**
 * TokenRevocation - Record of a revoked capability token
 */
struct TokenRevocation {
    NodeID node_id;                  // Whose token was revoked
    Capability capability;           // Which capability was revoked
    RevocationReason reason;
    uint64_t revoked_at;            // When it was revoked
    NodeID revoker;                  // Who revoked it (issuer or admin)
    std::vector<uint8_t> context;    // Optional context (e.g., network_id)
    Signature signature;             // Signature by revoker
    
    /**
     * Check if this revocation matches a given token
     */
    bool matches_token(const CapabilityToken& token) const;
    
    /**
     * Serialize for signing or transmission
     */
    std::vector<uint8_t> to_bytes() const;
    static std::optional<TokenRevocation> from_bytes(const std::vector<uint8_t>& data);
    
    /**
     * Generate unique ID for this revocation (for deduplication)
     */
    Hash256 get_id() const;
};

/**
 * RevocationListEntry - Entry in the revocation list
 */
struct RevocationListEntry {
    TokenRevocation revocation;
    uint64_t added_at;               // When added to local list
    std::set<NodeID> witnesses;      // Nodes that confirmed this revocation
    uint32_t propagation_count;      // How many times we've forwarded this
    
    // Default constructor
    RevocationListEntry()
        : added_at(0)
        , propagation_count(0)
    {}
    
    RevocationListEntry(const TokenRevocation& rev)
        : revocation(rev)
        , added_at(0)
        , propagation_count(0)
    {
        auto now = std::chrono::system_clock::now();
        added_at = std::chrono::system_clock::to_time_t(now);
    }
};

/**
 * RevocationListUpdate - Gossip message for revocation list
 */
struct RevocationListUpdate {
    std::vector<TokenRevocation> revocations;
    uint64_t timestamp;
    NodeID source_node;
    Signature signature;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<RevocationListUpdate> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * TokenRevocationManager - Manages token revocation and gossip propagation
 * 
 * Design:
 * 1. Local revocation - Node can revoke tokens it issued
 * 2. Network-wide propagation - Revocations gossiped to all peers
 * 3. Deduplication - Track seen revocations to prevent loops
 * 4. Expiration - Old revocations expire after 30 days
 * 5. Verification - Revocations must be signed by authorized revoker
 * 
 * Gossip protocol:
 * - Periodic broadcast of revocation list (every 5 minutes)
 * - Incremental updates on new revocations
 * - Fanout = 3 (same as general gossip)
 * - Witnesses track propagation reliability
 */
class TokenRevocationManager {
public:
    TokenRevocationManager();
    ~TokenRevocationManager() = default;
    
    /**
     * Revoke a token for a specific node
     * @param node_id Node whose token to revoke
     * @param capability Which capability to revoke
     * @param reason Why it's being revoked
     * @param revoker Who is revoking (must have authority)
     * @param context Optional context (e.g., network_id)
     * @return The revocation record if successful
     */
    std::optional<TokenRevocation> revoke_token(
        const NodeID& node_id,
        Capability capability,
        RevocationReason reason,
        const NodeID& revoker,
        const std::vector<uint8_t>& context = {}
    );
    
    /**
     * Check if a token has been revoked
     * @param token The token to check
     * @return true if token is revoked
     */
    bool is_token_revoked(const CapabilityToken& token) const;
    
    /**
     * Check if a node has any revocations for a capability
     * @param node_id Node to check
     * @param capability Capability to check
     * @return true if node has active revocations
     */
    bool has_revocations(const NodeID& node_id, Capability capability) const;
    
    /**
     * Get all revocations for a node
     * @param node_id Node to query
     * @return Vector of revocation records
     */
    std::vector<TokenRevocation> get_revocations_for(const NodeID& node_id) const;
    
    /**
     * Get recent revocations (for gossip)
     * @param since_timestamp Only revocations after this time
     * @param max_count Maximum number to return
     * @return Vector of recent revocations
     */
    std::vector<TokenRevocation> get_recent_revocations(
        uint64_t since_timestamp = 0,
        size_t max_count = 100
    ) const;
    
    /**
     * Process a revocation from another node (via gossip)
     * @param revocation The revocation record
     * @param source_node Which node sent it
     * @return true if revocation was accepted
     */
    bool process_revocation(const TokenRevocation& revocation, const NodeID& source_node);
    
    /**
     * Process a batch revocation list update
     * @param update The batch update
     * @return Number of new revocations accepted
     */
    size_t process_revocation_list(const RevocationListUpdate& update);
    
    /**
     * Create a revocation list update for gossip
     * @param include_all If true, include all revocations; if false, only recent
     * @return Revocation list update ready for signing and transmission
     */
    RevocationListUpdate create_revocation_list(bool include_all = false) const;
    
    /**
     * Verify a revocation is properly signed
     * @param revocation The revocation to verify
     * @param revoker_public_key Public key of the revoker
     * @return true if signature is valid
     */
    static bool verify_revocation(
        const TokenRevocation& revocation,
        const PublicKey& revoker_public_key
    );
    
    /**
     * Assign a signature to a revocation
     * @param revocation The revocation to sign (modified in place)
     * @param signature The signature to apply (computed by caller)
     */
    static void sign_revocation(
        TokenRevocation& revocation,
        const Signature& signature
    );
    
    // Statistics and maintenance
    size_t revocation_count() const;
    size_t expired_revocation_count() const;
    void cleanup_expired_revocations();
    
    // Configuration
    void set_revocation_expiry_days(uint32_t days) { revocation_expiry_days_ = days; }
    void set_max_revocations_per_node(uint32_t max) { max_revocations_per_node_ = max; }
    
private:
    // Revocation storage
    std::map<Hash256, RevocationListEntry> revocations_;  // By revocation ID
    std::map<NodeID, std::set<Hash256>> revocations_by_node_;  // Index by node
    std::map<Capability, std::set<Hash256>> revocations_by_capability_;  // Index by capability
    
    // Seen revocations (for deduplication)
    std::set<Hash256> seen_revocations_;
    
    // Configuration
    uint32_t revocation_expiry_days_;
    uint32_t max_revocations_per_node_;
    uint32_t max_propagation_count_;
    
    // Helpers
    bool is_revocation_expired(const TokenRevocation& revocation) const;
    bool should_accept_revocation(const TokenRevocation& revocation) const;
    void add_revocation_to_indexes(const Hash256& id, const TokenRevocation& revocation);
    void remove_revocation_from_indexes(const Hash256& id, const TokenRevocation& revocation);
    uint64_t current_timestamp() const;
};

/**
 * Helper to convert RevocationReason to string
 */
const char* revocation_reason_to_string(RevocationReason reason);

} // namespace cashew::security
