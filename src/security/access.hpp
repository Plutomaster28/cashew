#pragma once

#include "cashew/common.hpp"
#include "core/keys/key.hpp"
#include "core/ledger/state.hpp"
#include <string>
#include <vector>
#include <optional>

namespace cashew::security {

/**
 * Capability - What a node can do
 * 
 * Capabilities are determined by:
 * - Key ownership (type and count)
 * - Reputation score
 * - Network membership
 * - PoW/PoStake contributions
 */
enum class Capability {
    // Free (no keys required)
    VIEW_CONTENT,          // Browse and read Things
    DISCOVER_NETWORKS,     // See available networks
    RELAY_TRAFFIC,         // Participate in routing (basic)
    
    // Requires keys (anti-bot)
    POST_CONTENT,          // Create/update content in forums/chats
    VOTE_ON_CONTENT,       // Upvote/downvote
    CREATE_IDENTITY,       // Register new identity
    
    // Requires specific keys
    HOST_THINGS,           // Requires SERVICE keys
    JOIN_NETWORKS,         // Requires NETWORK keys
    ROUTE_TRAFFIC,         // Requires ROUTING keys
    ISSUE_INVITATIONS,     // Requires NETWORK keys
    
    // Requires reputation
    VOUCH_FOR_NODES,       // Requires reputation > threshold
    CREATE_NETWORK,        // Requires reputation + keys
    MODERATE_CONTENT,      // Requires reputation in network
    
    // Admin capabilities
    REVOKE_KEYS,           // Founder only
    DISBAND_NETWORK        // Founder only
};

/**
 * AccessLevel - Hierarchical access levels
 */
enum class AccessLevel {
    ANONYMOUS,    // No identity, can only view
    IDENTIFIED,   // Has identity key, can post with proof-of-work
    KEYED,        // Has earned keys, full participation
    TRUSTED,      // High reputation, can vouch
    FOUNDER       // Network creator, admin powers
};

/**
 * CapabilityToken - Proof of capability
 * 
 * Used to prove a node has permission to perform an action.
 * Includes signature to prevent forgery.
 */
struct CapabilityToken {
    NodeID node_id;
    Capability capability;
    uint64_t issued_at;
    uint64_t expires_at;
    std::vector<uint8_t> context;  // Optional context (e.g., network_id)
    Signature signature;
    
    bool is_expired() const;
    bool is_valid_for(const Hash256& context_hash) const;
};

/**
 * AccessPolicy - Rules for granting capabilities
 */
struct AccessPolicy {
    Capability capability;
    
    // Key requirements
    std::optional<core::KeyType> required_key_type;
    uint32_t required_key_count;
    
    // Reputation requirements
    int32_t min_reputation;
    
    // Network membership requirements
    bool requires_network_membership;
    std::optional<std::string> required_role;  // "FOUNDER", "FULL", etc.
    
    // PoW requirements (for anonymous posting)
    bool requires_pow;
    uint32_t pow_difficulty;
    
    AccessPolicy()
        : required_key_count(0), min_reputation(0),
          requires_network_membership(false),
          requires_pow(false), pow_difficulty(0) {}
};

/**
 * AccessRequest - Request to perform an action
 */
struct AccessRequest {
    NodeID requester;
    Capability capability;
    std::optional<Hash256> network_id;  // For network-specific actions
    std::optional<Hash256> thing_id;    // For Thing-specific actions
    std::optional<std::vector<uint8_t>> pow_solution;  // For anonymous posting
};

/**
 * AccessDecision - Result of access check
 */
struct AccessDecision {
    bool granted;
    std::string reason;
    std::optional<CapabilityToken> token;  // If granted
    
    static AccessDecision allow(const std::string& reason);
    static AccessDecision deny(const std::string& reason);
};

/**
 * AccessControl - Capability-based permission system
 * 
 * Core design principles:
 * 1. Free viewing - Anyone can browse content without keys
 * 2. Key-gated participation - Keys required for posting/hosting
 * 3. Reputation-based trust - High rep enables vouching
 * 4. Network-specific roles - Founders have special powers
 * 5. Anti-bot measures - PoW required for anonymous posting
 * 
 * This implements the access control model described in your notes:
 * "Viewing is free, but to *do* stuff you need keys"
 */
class AccessControl {
public:
    AccessControl(ledger::StateManager& state_manager);
    ~AccessControl() = default;
    
    // Policy management
    void set_policy(Capability capability, const AccessPolicy& policy);
    AccessPolicy get_policy(Capability capability) const;
    
    // Access checks
    AccessDecision check_access(const AccessRequest& request) const;
    
    bool can_view_content(const NodeID& node_id) const;
    bool can_post_content(const NodeID& node_id) const;
    bool can_post_anonymously(const std::vector<uint8_t>& pow_solution) const;
    
    bool can_host_things(const NodeID& node_id) const;
    bool can_join_network(const NodeID& node_id, const Hash256& network_id) const;
    bool can_route_traffic(const NodeID& node_id) const;
    
    bool can_vouch_for_node(const NodeID& voucher, const NodeID& vouchee) const;
    bool can_create_network(const NodeID& node_id) const;
    bool can_issue_invitation(const NodeID& node_id, const Hash256& network_id) const;
    
    bool can_moderate_content(const NodeID& node_id, const Hash256& network_id) const;
    bool can_revoke_keys(const NodeID& node_id, const Hash256& network_id) const;
    bool can_disband_network(const NodeID& node_id, const Hash256& network_id) const;
    
    // Token management
    std::optional<CapabilityToken> issue_token(const NodeID& node_id, 
                                                Capability capability,
                                                const std::optional<Hash256>& context = std::nullopt) const;
    
    bool verify_token(const CapabilityToken& token) const;
    
    // Access level determination
    AccessLevel get_access_level(const NodeID& node_id) const;
    AccessLevel get_access_level_in_network(const NodeID& node_id, 
                                             const Hash256& network_id) const;
    
    // Statistics
    size_t count_nodes_with_capability(Capability capability) const;
    
private:
    ledger::StateManager& state_manager_;
    std::map<Capability, AccessPolicy> policies_;
    
    // Default policies
    void initialize_default_policies();
    
    // Policy checks
    bool check_key_requirements(const NodeID& node_id, const AccessPolicy& policy) const;
    bool check_reputation_requirements(const NodeID& node_id, const AccessPolicy& policy) const;
    bool check_network_requirements(const NodeID& node_id, const Hash256& network_id, 
                                     const AccessPolicy& policy) const;
    bool check_pow_requirements(const std::vector<uint8_t>& pow_solution, 
                                 const AccessPolicy& policy) const;
    
    // Helpers
    bool is_network_founder(const NodeID& node_id, const Hash256& network_id) const;
    bool has_any_keys(const NodeID& node_id) const;
};

} // namespace cashew::security
