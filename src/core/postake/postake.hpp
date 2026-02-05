#pragma once

#include "cashew/common.hpp"
#include "core/keys/key.hpp"
#include "core/ledger/state.hpp"
#include <vector>
#include <map>
#include <optional>

namespace cashew::postake {

/**
 * ContributionType - Types of contributions that earn keys
 */
enum class ContributionType {
    UPTIME,           // Node online and responsive
    BANDWIDTH,        // Routing traffic for others
    STORAGE,          // Hosting Things
    ROUTING_QUALITY,  // Successful multi-hop routing
    EPOCH_WITNESS     // Participating in epoch consensus
};

/**
 * ContributionMetrics - Measurements of a node's contributions
 */
struct ContributionMetrics {
    NodeID node_id;
    
    // Uptime tracking (seconds)
    uint64_t total_uptime;
    uint64_t last_seen;
    uint64_t first_seen;
    
    // Bandwidth tracking (bytes)
    uint64_t bytes_routed;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    
    // Storage tracking
    uint32_t things_hosted;
    uint64_t storage_bytes_provided;
    
    // Routing quality
    uint32_t successful_routes;
    uint32_t failed_routes;
    float routing_reliability;  // 0.0 to 1.0
    
    // Epoch participation
    uint32_t epochs_witnessed;
    uint32_t epochs_missed;
    
    ContributionMetrics()
        : total_uptime(0), last_seen(0), first_seen(0),
          bytes_routed(0), bytes_sent(0), bytes_received(0),
          things_hosted(0), storage_bytes_provided(0),
          successful_routes(0), failed_routes(0), routing_reliability(1.0f),
          epochs_witnessed(0), epochs_missed(0) {}
    
    float uptime_percentage() const;
    float routing_success_rate() const;
};

/**
 * ContributionScore - Calculated score for key earning
 */
struct ContributionScore {
    NodeID node_id;
    
    uint32_t uptime_score;
    uint32_t bandwidth_score;
    uint32_t storage_score;
    uint32_t routing_score;
    uint32_t witness_score;
    
    uint32_t total_score;
    
    ContributionScore()
        : uptime_score(0), bandwidth_score(0), storage_score(0),
          routing_score(0), witness_score(0), total_score(0) {}
    
    void calculate_total();
};

/**
 * KeyEarningRate - Rate at which contributions earn keys
 */
struct KeyEarningRate {
    core::KeyType key_type;
    
    // Points required per key
    uint32_t points_per_key;
    
    // Maximum keys per epoch
    uint32_t max_per_epoch;
    
    // Minimum contribution score required
    uint32_t min_score_required;
};

/**
 * EpochContribution - Contributions within a specific epoch
 */
struct EpochContribution {
    uint64_t epoch;
    NodeID node_id;
    
    ContributionMetrics metrics;
    ContributionScore score;
    
    uint32_t keys_earned;
    core::KeyType key_type_earned;
    
    uint64_t recorded_at;
};

/**
 * PoStakeReward - Keys earned through contribution
 */
struct PoStakeReward {
    NodeID node_id;
    uint64_t epoch;
    core::KeyType key_type;
    uint32_t key_count;
    uint64_t awarded_at;
    Hash256 proof_hash;  // Hash of contribution metrics
};

/**
 * ContributionTracker - Tracks ongoing contributions
 */
class ContributionTracker {
public:
    ContributionTracker() = default;
    ~ContributionTracker() = default;
    
    // Uptime tracking
    void record_node_online(const NodeID& node_id);
    void record_node_offline(const NodeID& node_id);
    void update_uptime(const NodeID& node_id, uint64_t seconds);
    
    // Bandwidth tracking
    void record_bytes_routed(const NodeID& node_id, uint64_t bytes);
    void record_traffic(const NodeID& node_id, uint64_t sent, uint64_t received);
    
    // Storage tracking
    void record_thing_hosted(const NodeID& node_id, uint64_t size_bytes);
    void record_thing_removed(const NodeID& node_id, uint64_t size_bytes);
    
    // Routing quality
    void record_successful_route(const NodeID& node_id);
    void record_failed_route(const NodeID& node_id);
    
    // Epoch participation
    void record_epoch_witness(const NodeID& node_id, uint64_t epoch);
    void record_epoch_missed(const NodeID& node_id, uint64_t epoch);
    
    // Query metrics
    ContributionMetrics get_metrics(const NodeID& node_id) const;
    std::vector<NodeID> get_active_contributors() const;
    
    // Cleanup
    void reset_metrics(const NodeID& node_id);
    void cleanup_inactive_nodes(uint64_t inactive_threshold = 86400);
    
private:
    std::map<NodeID, ContributionMetrics> metrics_;
    std::map<NodeID, bool> online_status_;
    std::map<NodeID, uint64_t> online_since_;
    
    uint64_t current_timestamp() const;
    void update_routing_reliability(ContributionMetrics& metrics);
};

/**
 * PoStakeEngine - Proof-of-Stake contribution system
 * 
 * Nodes earn keys by contributing to the network:
 * - Uptime: Being online and responsive
 * - Bandwidth: Routing traffic for others
 * - Storage: Hosting Things
 * - Routing Quality: Successful content delivery
 * - Epoch Participation: Witnessing network state
 * 
 * Design principles:
 * - Fair: Raspberry Pi can compete with powerful servers
 * - Transparent: Clear rules for earning
 * - Automated: No manual key distribution
 * - Decay-resistant: Active nodes maintain keys
 * - Sybil-resistant: Requires actual resource contribution
 */
class PoStakeEngine {
public:
    PoStakeEngine(ledger::StateManager& state_manager);
    ~PoStakeEngine() = default;
    
    // Tracker access
    ContributionTracker& get_tracker() { return tracker_; }
    const ContributionTracker& get_tracker() const { return tracker_; }
    
    // Scoring
    ContributionScore calculate_score(const NodeID& node_id) const;
    ContributionScore calculate_score(const ContributionMetrics& metrics) const;
    
    // Key earning rates
    void set_earning_rate(core::KeyType key_type, const KeyEarningRate& rate);
    KeyEarningRate get_earning_rate(core::KeyType key_type) const;
    
    // Epoch processing
    void process_epoch(uint64_t epoch);
    std::vector<PoStakeReward> calculate_epoch_rewards(uint64_t epoch) const;
    
    // Award keys
    bool award_keys(const PoStakeReward& reward);
    
    // Query contributions
    std::optional<EpochContribution> get_epoch_contribution(const NodeID& node_id, uint64_t epoch) const;
    std::vector<EpochContribution> get_node_history(const NodeID& node_id) const;
    
    // Rankings
    std::vector<NodeID> get_top_contributors(ContributionType type, uint32_t count = 10) const;
    
    // Statistics
    uint32_t get_total_keys_awarded(uint64_t epoch) const;
    uint32_t get_average_contribution_score() const;
    
private:
    ledger::StateManager& state_manager_;
    ContributionTracker tracker_;
    
    // Earning rates for each key type
    std::map<core::KeyType, KeyEarningRate> earning_rates_;
    
    // Epoch history
    std::map<uint64_t, std::vector<EpochContribution>> epoch_contributions_;
    std::map<uint64_t, std::vector<PoStakeReward>> epoch_rewards_;
    
    // Scoring weights
    static constexpr float UPTIME_WEIGHT = 0.3f;
    static constexpr float BANDWIDTH_WEIGHT = 0.25f;
    static constexpr float STORAGE_WEIGHT = 0.25f;
    static constexpr float ROUTING_WEIGHT = 0.15f;
    static constexpr float WITNESS_WEIGHT = 0.05f;
    
    // Helpers
    void initialize_default_rates();
    uint32_t calculate_uptime_score(const ContributionMetrics& metrics) const;
    uint32_t calculate_bandwidth_score(const ContributionMetrics& metrics) const;
    uint32_t calculate_storage_score(const ContributionMetrics& metrics) const;
    uint32_t calculate_routing_score(const ContributionMetrics& metrics) const;
    uint32_t calculate_witness_score(const ContributionMetrics& metrics) const;
    
    core::KeyType determine_key_type(const ContributionScore& score) const;
    uint32_t calculate_key_count(const ContributionScore& score, core::KeyType key_type) const;
    
    Hash256 hash_contribution(const ContributionMetrics& metrics) const;
};

} // namespace cashew::postake
