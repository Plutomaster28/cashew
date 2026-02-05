#pragma once

#include "cashew/common.hpp"
#include "core/ledger/state.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace cashew::reputation {

/**
 * ReputationAction - Actions that affect reputation
 */
enum class ReputationAction {
    // Positive actions
    HOST_THING,              // +10: Hosting a Thing
    CONTRIBUTE_BANDWIDTH,    // +5: Routing traffic
    SUBMIT_POW,              // +2: PoW solution
    PROVIDE_POSTAKE,         // +15: Uptime/storage contribution
    VOUCH_SUCCESSFUL,        // +20: Vouched node behaves well
    
    // Negative actions
    OFFLINE_PROLONGED,       // -10: Extended downtime
    NETWORK_VIOLATION,       // -50: Breaking network rules
    SPAM_DETECTED,           // -30: Posting spam
    VOUCH_FAILED,            // -40: Vouched node misbehaves
    CONTENT_REMOVED          // -20: Content moderation removal
};

/**
 * Attestation - Signed statement about another node
 * 
 * Used for vouching, trust establishment, reputation feedback
 */
struct Attestation {
    NodeID attester;           // Who is making the statement
    NodeID subject;            // Who the statement is about
    int32_t score_delta;       // Reputation change (-100 to +100)
    std::string statement;     // Human-readable reason
    uint64_t timestamp;
    uint64_t expires_at;       // Attestations expire
    Signature signature;
    
    bool is_expired() const;
    bool is_vouch() const;     // Positive attestation
    bool is_denouncement() const;  // Negative attestation
};

/**
 * TrustEdge - Directed trust relationship
 */
struct TrustEdge {
    NodeID from;
    NodeID to;
    float trust_weight;        // 0.0 to 1.0
    uint64_t established_at;
    uint64_t last_updated;
    
    bool is_strong() const { return trust_weight >= 0.7f; }
    bool is_weak() const { return trust_weight < 0.3f; }
};

/**
 * VouchRecord - Record of vouching relationship
 */
struct VouchRecord {
    NodeID voucher;
    NodeID vouchee;
    uint64_t vouched_at;
    bool still_active;
    int32_t vouchee_reputation_at_vouch;
    int32_t vouchee_current_reputation;
    
    // Voucher's reputation is affected by vouchee's behavior
    int32_t calculate_voucher_impact() const;
};

/**
 * ReputationHistory - Track reputation changes over time
 */
struct ReputationEvent {
    uint64_t timestamp;
    ReputationAction action;
    int32_t score_delta;
    int32_t score_after;
    std::optional<NodeID> related_node;  // For vouching
    std::string details;
};

/**
 * ReputationScore - Detailed breakdown of a node's reputation
 */
struct ReputationScore {
    NodeID node_id;
    int32_t total_score;
    
    // Component scores
    int32_t hosting_score;
    int32_t contribution_score;
    int32_t vouching_score;
    int32_t penalty_score;
    
    // Metrics
    uint32_t things_hosted;
    uint64_t bandwidth_contributed;
    uint32_t successful_vouches;
    uint32_t failed_vouches;
    uint32_t violations;
    
    // History
    std::vector<ReputationEvent> recent_events;  // Last 100 events
    
    float trust_level() const;  // 0.0 to 1.0
    bool is_trustworthy() const { return total_score >= 100; }
    bool is_suspicious() const { return total_score < -50; }
};

/**
 * TrustGraph - Network of trust relationships
 * 
 * Models trust propagation through the network.
 * If A trusts B, and B trusts C, then A might trust C (transitive).
 */
class TrustGraph {
public:
    TrustGraph() = default;
    ~TrustGraph() = default;
    
    // Edge management
    void add_edge(const NodeID& from, const NodeID& to, float weight);
    void remove_edge(const NodeID& from, const NodeID& to);
    void update_edge_weight(const NodeID& from, const NodeID& to, float weight);
    
    // Trust queries
    std::optional<float> get_direct_trust(const NodeID& from, const NodeID& to) const;
    float calculate_transitive_trust(const NodeID& from, const NodeID& to, uint32_t max_hops = 3) const;
    
    std::vector<NodeID> get_trusted_by(const NodeID& node) const;
    std::vector<NodeID> get_trusts(const NodeID& node) const;
    
    // Community detection
    std::set<NodeID> find_trust_community(const NodeID& node, float min_trust = 0.5f) const;
    
    // Maintenance
    void decay_edge_weights(float decay_factor = 0.95f);  // Periodic decay
    void prune_weak_edges(float threshold = 0.1f);
    
private:
    std::map<NodeID, std::map<NodeID, TrustEdge>> edges_;
    
    // Helper for transitive trust calculation
    float calculate_path_trust(const std::vector<NodeID>& path) const;
};

/**
 * ReputationManager - Central reputation system
 * 
 * Manages reputation scores, attestations, vouching, and trust graph.
 * 
 * Design principles:
 * - Reputation is earned through contribution (hosting, routing, uptime)
 * - Vouching creates accountability (voucher shares risk)
 * - Negative actions have stronger impact than positive (easier to lose trust)
 * - Reputation decays slowly without activity
 * - Trust is transitive but diminishes with distance
 */
class ReputationManager {
public:
    ReputationManager(ledger::StateManager& state_manager);
    ~ReputationManager() = default;
    
    // Reputation scoring
    int32_t get_reputation(const NodeID& node_id) const;
    ReputationScore get_detailed_score(const NodeID& node_id) const;
    
    void record_action(const NodeID& node_id, ReputationAction action,
                      const std::optional<NodeID>& related_node = std::nullopt);
    
    void apply_score_delta(const NodeID& node_id, int32_t delta, const std::string& reason);
    
    // Attestation management
    bool create_attestation(const Attestation& attestation);
    bool verify_attestation(const Attestation& attestation) const;
    std::vector<Attestation> get_attestations_for(const NodeID& subject) const;
    std::vector<Attestation> get_attestations_by(const NodeID& attester) const;
    
    // Vouching system
    bool vouch_for_node(const NodeID& voucher, const NodeID& vouchee);
    bool can_vouch(const NodeID& voucher, const NodeID& vouchee) const;
    std::vector<VouchRecord> get_vouches_by(const NodeID& voucher) const;
    std::vector<VouchRecord> get_vouches_for(const NodeID& vouchee) const;
    
    void update_vouch_impacts();  // Periodic: update voucher reputation based on vouchee behavior
    
    // Trust graph
    TrustGraph& get_trust_graph() { return trust_graph_; }
    const TrustGraph& get_trust_graph() const { return trust_graph_; }
    
    void rebuild_trust_graph();  // Rebuild from attestations and vouches
    
    // Rankings
    std::vector<NodeID> get_top_reputation(uint32_t count = 10) const;
    std::vector<NodeID> get_suspicious_nodes() const;
    
    // Statistics
    int32_t get_average_reputation() const;
    int32_t get_median_reputation() const;
    size_t count_trustworthy_nodes() const;
    
    // Maintenance
    void decay_reputation();  // Periodic: slowly decay all scores toward zero
    void cleanup_expired_attestations();
    
private:
    ledger::StateManager& state_manager_;
    
    // Reputation data
    std::map<NodeID, ReputationScore> scores_;
    std::map<NodeID, std::vector<Attestation>> attestations_;  // By subject
    std::map<NodeID, std::vector<VouchRecord>> vouches_;       // By voucher
    
    TrustGraph trust_graph_;
    
    // Scoring parameters
    static constexpr int32_t VOUCH_REPUTATION_REQUIREMENT = 100;
    static constexpr int32_t MAX_VOUCHES_PER_NODE = 5;
    static constexpr float REPUTATION_DECAY_RATE = 0.99f;  // 1% decay per epoch
    static constexpr int32_t REPUTATION_FLOOR = -1000;
    static constexpr int32_t REPUTATION_CEILING = 10000;
    
    // Helpers
    void initialize_score(const NodeID& node_id);
    int32_t get_action_score(ReputationAction action) const;
    void update_trust_from_attestation(const Attestation& attestation);
    void clamp_reputation(int32_t& score) const;
};

} // namespace cashew::reputation
