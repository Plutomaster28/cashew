#pragma once

#include "cashew/common.hpp"
#include "core/keys/key.hpp"
#include "core/ledger/state.hpp"
#include <vector>
#include <map>
#include <optional>

namespace cashew::decay {

/**
 * DecayReason - Why something decayed
 */
enum class DecayReason {
    INACTIVITY,          // Node offline too long
    EXPIRATION,          // Reached time limit
    RESOURCE_SHORTAGE,   // Not enough storage/bandwidth
    POOR_PERFORMANCE,    // Routing failures, downtime
    VIOLATION            // Network rules broken
};

/**
 * KeyDecayEvent - Record of a key decay
 */
struct KeyDecayEvent {
    NodeID node_id;
    core::KeyType key_type;
    uint32_t keys_decayed;
    DecayReason reason;
    uint64_t decayed_at;
    uint64_t epoch;
};

/**
 * ThingDecayEvent - Record of a Thing removal
 */
struct ThingDecayEvent {
    ContentHash content_hash;
    std::vector<NodeID> hosts_removed;
    DecayReason reason;
    uint64_t decayed_at;
};

/**
 * DecayPolicy - Rules for decay
 */
struct DecayPolicy {
    core::KeyType key_type;
    
    // Time-based decay
    uint64_t max_age_seconds;           // Max lifetime (30 days)
    uint64_t inactivity_threshold;      // Decay after this much inactivity
    
    // Activity-based decay
    bool requires_activity;             // Must be used
    uint32_t min_actions_per_epoch;     // Minimum activity level
    
    // Performance-based decay
    bool requires_performance;          // Must maintain quality
    float min_success_rate;             // Minimum success rate (0.0-1.0)
    
    DecayPolicy()
        : max_age_seconds(30 * 24 * 60 * 60),  // 30 days
          inactivity_threshold(7 * 24 * 60 * 60),  // 7 days
          requires_activity(false),
          min_actions_per_epoch(0),
          requires_performance(false),
          min_success_rate(0.5f) {}
};

/**
 * ThingDecayPolicy - Rules for Thing cleanup
 */
struct ThingDecayPolicy {
    uint64_t max_age_seconds;           // Remove if unused this long
    uint32_t min_hosts_required;        // Remove if below this redundancy
    uint64_t inactivity_threshold;      // No access for this long
    
    ThingDecayPolicy()
        : max_age_seconds(90 * 24 * 60 * 60),  // 90 days
          min_hosts_required(2),
          inactivity_threshold(30 * 24 * 60 * 60) {}  // 30 days
};

/**
 * NodeActivityTracker - Track node activity for decay decisions
 */
struct NodeActivity {
    NodeID node_id;
    uint64_t last_seen;
    uint64_t last_key_use;
    std::map<core::KeyType, uint64_t> last_use_by_type;
    std::map<core::KeyType, uint32_t> actions_this_epoch;
    
    bool is_inactive(uint64_t threshold) const;
    bool has_used_key_type(core::KeyType type, uint64_t threshold) const;
};

/**
 * ThingActivityTracker - Track Thing access for decay decisions
 */
struct ThingActivity {
    ContentHash content_hash;
    uint64_t created_at;
    uint64_t last_accessed;
    uint32_t access_count;
    std::vector<NodeID> current_hosts;
    
    bool is_inactive(uint64_t threshold) const;
    bool meets_redundancy(uint32_t min_hosts) const;
};

/**
 * DecayScheduler - Manages time-based decay of keys and Things
 * 
 * Runs periodic checks (per epoch) to:
 * - Expire old keys
 * - Remove inactive keys
 * - Clean up unused Things
 * - Enforce performance requirements
 * 
 * Design principles:
 * - Gradual decay: Keys don't disappear instantly
 * - Activity-based: Active use prevents decay
 * - Fair: Same rules for everyone
 * - Transparent: Clear reasons for decay
 * - Reversible: Can earn back decayed keys
 */
class DecayScheduler {
public:
    DecayScheduler(ledger::StateManager& state_manager);
    ~DecayScheduler() = default;
    
    // Policy management
    void set_key_policy(core::KeyType key_type, const DecayPolicy& policy);
    DecayPolicy get_key_policy(core::KeyType key_type) const;
    
    void set_thing_policy(const ThingDecayPolicy& policy);
    ThingDecayPolicy get_thing_policy() const;
    
    // Activity tracking
    void record_node_activity(const NodeID& node_id);
    void record_key_use(const NodeID& node_id, core::KeyType key_type);
    void record_thing_access(const ContentHash& content_hash);
    
    // Epoch processing
    void process_epoch(uint64_t epoch);
    
    std::vector<KeyDecayEvent> check_key_decay(uint64_t epoch);
    std::vector<ThingDecayEvent> check_thing_decay();
    
    // Apply decay
    void apply_key_decay(const KeyDecayEvent& event);
    void apply_thing_decay(const ThingDecayEvent& event);
    
    // Query decay history
    std::vector<KeyDecayEvent> get_key_decay_history(const NodeID& node_id) const;
    std::vector<ThingDecayEvent> get_thing_decay_history() const;
    
    // Statistics
    uint32_t get_total_keys_decayed(uint64_t epoch) const;
    uint32_t get_total_things_decayed() const;
    std::map<DecayReason, uint32_t> get_decay_reasons_breakdown() const;
    
    // Manual triggers
    void force_decay_check();
    void cleanup_inactive_nodes(uint64_t threshold = 30 * 24 * 60 * 60);
    
private:
    ledger::StateManager& state_manager_;
    
    // Policies
    std::map<core::KeyType, DecayPolicy> key_policies_;
    ThingDecayPolicy thing_policy_;
    
    // Activity tracking
    std::map<NodeID, NodeActivity> node_activities_;
    std::map<ContentHash, ThingActivity> thing_activities_;
    
    // Decay history
    std::map<uint64_t, std::vector<KeyDecayEvent>> key_decay_by_epoch_;
    std::vector<ThingDecayEvent> thing_decay_history_;
    
    // Helpers
    void initialize_default_policies();
    
    bool should_decay_key(const NodeID& node_id, core::KeyType key_type, 
                          const DecayPolicy& policy, DecayReason& reason) const;
    
    bool should_decay_thing(const ContentHash& content_hash, 
                           const ThingDecayPolicy& policy, DecayReason& reason) const;
    
    uint64_t current_timestamp() const;
    void update_node_activity_from_state(const NodeID& node_id);
    void update_thing_activity_from_state(const ContentHash& content_hash);
};

} // namespace cashew::decay
