#pragma once

#include "cashew/common.hpp"
#include "postake.hpp"
#include "core/pow/pow.hpp"
#include "core/keys/key.hpp"
#include "core/ledger/state.hpp"
#include "crypto/blake3.hpp"
#include <memory>
#include <optional>

namespace cashew::postake {

/**
 * KeyIssuanceMethod - How a key was earned
 */
enum class KeyIssuanceMethod {
    POW_ONLY,          // Pure Proof-of-Work (cold start)
    POSTAKE_ONLY,      // Pure Proof-of-Stake (established node)
    HYBRID             // Combination of both
};

/**
 * HybridIssuanceRecord - Record of key issuance
 */
struct HybridIssuanceRecord {
    NodeID node_id;
    KeyIssuanceMethod method;
    
    // PoW data (if applicable)
    std::optional<core::PowSolution> pow_solution;
    
    // PoStake data (if applicable)
    std::optional<ContributionScore> contribution_score;
    
    // Resulting key
    core::KeyType key_type;
    uint32_t key_count;
    uint64_t issued_at;
    uint64_t epoch;
    
    Hash256 record_hash;  // Hash of the issuance record
    
    std::vector<uint8_t> to_bytes() const;
    Hash256 compute_hash() const;
};

/**
 * HybridPolicy - Policy for hybrid PoW/PoStake coordination
 */
struct HybridPolicy {
    // New nodes (no reputation/contribution history)
    bool require_pow_for_new_nodes = true;
    uint32_t new_node_pow_keys = 5;  // Keys earned via PoW to bootstrap
    
    // Established nodes (with contribution history)
    bool allow_postake_only = true;
    uint32_t min_contribution_score = 100;  // Minimum score to skip PoW
    
    // Hybrid mode (combine both)
    bool enable_hybrid_bonus = true;
    float hybrid_multiplier = 1.5f;  // 50% bonus for nodes that do both
    
    // Weighting (when both are required)
    float pow_weight = 0.4f;    // 40% weight on PoW
    float postake_weight = 0.6f;  // 60% weight on PoStake
    
    // Anti-spam
    uint32_t max_keys_per_epoch = 10;  // Global limit per node per epoch
    uint64_t min_seconds_between_issuance = 60;  // Rate limiting
    
    HybridPolicy() = default;
    
    bool is_valid() const {
        return (pow_weight + postake_weight) == 1.0f;
    }
};

/**
 * HybridCoordinator - Coordinates PoW and PoStake for key issuance
 * 
 * Design goals:
 * - Bootstrap: New nodes can join via PoW (no history required)
 * - Fairness: Established nodes can earn via PoStake (reward contribution)
 * - Flexibility: Nodes can use PoW, PoStake, or both
 * - Anti-Sybil: PoW prevents free key creation, PoStake rewards real work
 * - Incentives: Hybrid mode gives bonuses to encourage full participation
 */
class HybridCoordinator {
public:
    HybridCoordinator(
        ledger::StateManager& state_manager,
        core::ProofOfWork& pow_engine,
        PoStakeEngine& postake_engine
    );
    ~HybridCoordinator() = default;
    
    // Policy management
    void set_policy(const HybridPolicy& policy);
    HybridPolicy get_policy() const { return policy_; }
    
    // Key issuance requests
    std::optional<HybridIssuanceRecord> request_keys_via_pow(
        const NodeID& node_id,
        const core::PowSolution& pow_solution,
        core::KeyType key_type,
        uint32_t key_count
    );
    
    std::optional<HybridIssuanceRecord> request_keys_via_postake(
        const NodeID& node_id,
        core::KeyType key_type,
        uint32_t key_count
    );
    
    std::optional<HybridIssuanceRecord> request_keys_hybrid(
        const NodeID& node_id,
        const core::PowSolution& pow_solution,
        core::KeyType key_type,
        uint32_t key_count
    );
    
    // Validation
    bool validate_issuance(const HybridIssuanceRecord& record) const;
    bool can_issue_keys(
        const NodeID& node_id,
        uint32_t key_count,
        uint64_t current_epoch
    ) const;
    
    // Query
    std::vector<HybridIssuanceRecord> get_issuance_history(
        const NodeID& node_id
    ) const;
    
    uint32_t get_keys_issued_in_epoch(
        const NodeID& node_id,
        uint64_t epoch
    ) const;
    
    std::optional<uint64_t> get_last_issuance_time(
        const NodeID& node_id
    ) const;
    
    // Determine issuance method for node
    KeyIssuanceMethod recommend_method(const NodeID& node_id) const;
    
    // Statistics
    uint64_t total_keys_issued() const { return total_keys_issued_; }
    uint64_t pow_issuances() const { return pow_issuances_; }
    uint64_t postake_issuances() const { return postake_issuances_; }
    uint64_t hybrid_issuances() const { return hybrid_issuances_; }
    
private:
    ledger::StateManager& state_manager_;
    core::ProofOfWork& pow_engine_;
    PoStakeEngine& postake_engine_;
    
    HybridPolicy policy_;
    
    // Issuance tracking
    std::map<NodeID, std::vector<HybridIssuanceRecord>> issuance_history_;
    std::map<NodeID, uint64_t> last_issuance_time_;
    std::map<std::pair<NodeID, uint64_t>, uint32_t> epoch_key_counts_;
    
    // Statistics
    uint64_t total_keys_issued_;
    uint64_t pow_issuances_;
    uint64_t postake_issuances_;
    uint64_t hybrid_issuances_;
    
    // Helper methods
    bool is_new_node(const NodeID& node_id) const;
    uint32_t calculate_hybrid_bonus(uint32_t base_keys) const;
    bool check_rate_limit(const NodeID& node_id) const;
    void record_issuance(const HybridIssuanceRecord& record);
    
    uint64_t current_timestamp() const;
    uint64_t current_epoch() const;
};

} // namespace cashew::postake
