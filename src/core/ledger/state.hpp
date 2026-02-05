#pragma once

#include "cashew/common.hpp"
#include "core/ledger/ledger.hpp"
#include "core/keys/key.hpp"
#include "core/thing/thing.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace cashew::ledger {

/**
 * NodeState - Current state of a node
 */
struct NodeState {
    NodeID node_id;
    uint64_t joined_at;
    bool is_active;
    
    // Key balances
    std::map<core::KeyType, uint32_t> key_balances;
    
    // Network memberships
    std::set<Hash256> networks;
    
    // Things hosted
    std::set<ContentHash> hosted_things;
    
    // Reputation score
    int32_t reputation_score;
    
    // Contribution metrics
    uint64_t uptime_seconds;
    uint64_t bandwidth_contributed;
    uint32_t pow_solutions;
    uint32_t postake_contributions;
    
    NodeState()
        : joined_at(0), is_active(false), reputation_score(0),
          uptime_seconds(0), bandwidth_contributed(0),
          pow_solutions(0), postake_contributions(0) {}
    
    bool has_key_type(core::KeyType type, uint32_t min_count = 1) const;
    bool can_host_things() const;
    bool can_join_networks() const;
    bool can_route() const;
};

/**
 * NetworkState - Current state of a network
 */
struct NetworkState {
    Hash256 network_id;
    uint64_t created_at;
    bool is_active;
    
    std::set<NodeID> members;
    std::map<NodeID, std::string> member_roles;  // "FOUNDER", "FULL", "OBSERVER"
    
    std::optional<ContentHash> hosted_thing;
    
    NetworkState()
        : created_at(0), is_active(false) {}
    
    size_t member_count() const { return members.size(); }
    bool has_member(const NodeID& node_id) const;
    std::string get_member_role(const NodeID& node_id) const;
};

/**
 * ThingState - Current state of a Thing
 */
struct ThingState {
    ContentHash content_hash;
    uint64_t created_at;
    bool is_available;
    
    std::set<NodeID> hosts;
    std::set<Hash256> networks;
    
    uint64_t total_size_bytes;
    uint32_t replication_count;
    
    ThingState()
        : created_at(0), is_available(false),
          total_size_bytes(0), replication_count(0) {}
    
    size_t host_count() const { return hosts.size(); }
    bool is_hosted_by(const NodeID& node_id) const;
    bool meets_redundancy_requirements(uint32_t min_redundancy = 3) const;
};

/**
 * StateSnapshot - Point-in-time snapshot of network state
 */
struct StateSnapshot {
    uint64_t timestamp;
    uint64_t epoch;
    Hash256 ledger_hash;
    
    size_t total_nodes;
    size_t active_nodes;
    size_t total_networks;
    size_t total_things;
    uint64_t total_keys_issued;
    
    std::string to_string() const;
};

/**
 * StateManager - High-level interface for querying current network state
 * 
 * StateManager builds a view of the current state by processing
 * the event ledger. It provides efficient queries for:
 * - Node capabilities and key balances
 * - Network memberships
 * - Thing replication status
 * - Reputation scores
 * 
 * This is the main interface applications use to understand
 * "what is the current state of the network?"
 */
class StateManager {
public:
    StateManager(Ledger& ledger);
    ~StateManager() = default;
    
    // Rebuild state from ledger
    void rebuild_state();
    void apply_event(const LedgerEvent& event);
    
    // Node queries
    std::optional<NodeState> get_node_state(const NodeID& node_id) const;
    std::vector<NodeState> get_all_active_nodes() const;
    std::vector<NodeID> get_nodes_with_key_type(core::KeyType type, uint32_t min_count = 1) const;
    
    bool is_node_active(const NodeID& node_id) const;
    uint32_t get_node_key_balance(const NodeID& node_id, core::KeyType type) const;
    int32_t get_node_reputation(const NodeID& node_id) const;
    
    // Network queries
    std::optional<NetworkState> get_network_state(const Hash256& network_id) const;
    std::vector<NetworkState> get_all_active_networks() const;
    std::vector<Hash256> get_networks_for_node(const NodeID& node_id) const;
    std::vector<Hash256> get_networks_hosting_thing(const ContentHash& content_hash) const;
    
    bool is_network_active(const Hash256& network_id) const;
    bool is_node_in_network(const NodeID& node_id, const Hash256& network_id) const;
    
    // Thing queries
    std::optional<ThingState> get_thing_state(const ContentHash& content_hash) const;
    std::vector<ThingState> get_all_available_things() const;
    std::vector<NodeID> get_thing_hosts(const ContentHash& content_hash) const;
    
    bool is_thing_available(const ContentHash& content_hash) const;
    uint32_t get_thing_replication_count(const ContentHash& content_hash) const;
    
    // Capability checks (for access control)
    bool can_node_host_things(const NodeID& node_id) const;
    bool can_node_join_networks(const NodeID& node_id) const;
    bool can_node_route_traffic(const NodeID& node_id) const;
    bool can_node_post_content(const NodeID& node_id) const;  // Anti-bot check
    
    // Statistics
    StateSnapshot get_snapshot() const;
    size_t active_node_count() const { return nodes_.size(); }
    size_t active_network_count() const { return networks_.size(); }
    size_t available_thing_count() const { return things_.size(); }
    
    // Maintenance
    void update_node_activity();  // Mark inactive nodes
    void cleanup_stale_state();

private:
    Ledger& ledger_;
    
    // Current state caches
    std::map<NodeID, NodeState> nodes_;
    std::map<Hash256, NetworkState> networks_;
    std::map<ContentHash, ThingState> things_;
    
    // Last rebuild timestamp
    uint64_t last_rebuild_;
    
    // Helpers
    void apply_node_joined(const LedgerEvent& event);
    void apply_node_left(const LedgerEvent& event);
    void apply_key_issued(const LedgerEvent& event);
    void apply_key_revoked(const LedgerEvent& event);
    void apply_network_created(const LedgerEvent& event);
    void apply_network_member_added(const LedgerEvent& event);
    void apply_network_member_removed(const LedgerEvent& event);
    void apply_thing_replicated(const LedgerEvent& event);
    void apply_thing_removed(const LedgerEvent& event);
    void apply_reputation_updated(const LedgerEvent& event);
    void apply_pow_solution(const LedgerEvent& event);
    void apply_postake_contribution(const LedgerEvent& event);
    
    uint64_t current_timestamp() const;
};

} // namespace cashew::ledger
