#include "hybrid_coordinator.hpp"
#include "cashew/time_utils.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace cashew::postake {

// HybridIssuanceRecord methods
std::vector<uint8_t> HybridIssuanceRecord::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Node ID (32 bytes)
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    
    // Method
    data.push_back(static_cast<uint8_t>(method));
    
    // PoW solution (if present)
    uint8_t has_pow = pow_solution.has_value() ? 1 : 0;
    data.push_back(has_pow);
    if (has_pow) {
        auto& sol = pow_solution.value();
        data.insert(data.end(), sol.solution_hash.begin(), sol.solution_hash.end());
        // Add nonce (8 bytes)
        for (int i = 7; i >= 0; --i) {
            data.push_back((sol.nonce >> (i * 8)) & 0xFF);
        }
    }
    
    // PoStake score (if present)
    uint8_t has_postake = contribution_score.has_value() ? 1 : 0;
    data.push_back(has_postake);
    if (has_postake) {
        auto& score = contribution_score.value();
        // Total score (4 bytes)
        for (int i = 3; i >= 0; --i) {
            data.push_back((score.total_score >> (i * 8)) & 0xFF);
        }
    }
    
    // Key type
    data.push_back(static_cast<uint8_t>(key_type));
    
    // Key count (4 bytes)
    for (int i = 3; i >= 0; --i) {
        data.push_back((key_count >> (i * 8)) & 0xFF);
    }
    
    // Timestamps (8 bytes each)
    for (int i = 7; i >= 0; --i) {
        data.push_back((issued_at >> (i * 8)) & 0xFF);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back((epoch >> (i * 8)) & 0xFF);
    }
    
    return data;
}

Hash256 HybridIssuanceRecord::compute_hash() const {
    auto data = to_bytes();
    return crypto::Blake3::hash(data);
}

// HybridCoordinator methods
HybridCoordinator::HybridCoordinator(
    ledger::StateManager& state_manager,
    core::ProofOfWork& pow_engine,
    PoStakeEngine& postake_engine
)
    : state_manager_(state_manager),
      pow_engine_(pow_engine),
      postake_engine_(postake_engine),
      total_keys_issued_(0),
      pow_issuances_(0),
      postake_issuances_(0),
      hybrid_issuances_(0)
{
    spdlog::info("HybridCoordinator initialized");
}

void HybridCoordinator::set_policy(const HybridPolicy& policy) {
    if (!policy.is_valid()) {
        spdlog::error("Invalid HybridPolicy: weights must sum to 1.0");
        return;
    }
    policy_ = policy;
    spdlog::info("HybridPolicy updated");
}

uint64_t HybridCoordinator::current_timestamp() const {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

uint64_t HybridCoordinator::current_epoch() const {
    cashew::time::EpochManager epoch_mgr;
    return epoch_mgr.current_epoch();
}

bool HybridCoordinator::is_new_node(const NodeID& node_id) const {
    // Check if node has any contribution history
    auto metrics = postake_engine_.get_tracker().get_metrics(node_id);
    
    // Consider "new" if:
    // - No uptime recorded
    // - Never hosted anything
    // - No routing activity
    return (metrics.total_uptime == 0 &&
            metrics.things_hosted == 0 &&
            metrics.successful_routes == 0);
}

uint32_t HybridCoordinator::calculate_hybrid_bonus(uint32_t base_keys) const {
    if (!policy_.enable_hybrid_bonus) {
        return base_keys;
    }
    
    return static_cast<uint32_t>(base_keys * policy_.hybrid_multiplier);
}

bool HybridCoordinator::check_rate_limit(const NodeID& node_id) const {
    auto it = last_issuance_time_.find(node_id);
    if (it == last_issuance_time_.end()) {
        return true;  // No previous issuance
    }
    
    uint64_t elapsed = current_timestamp() - it->second;
    return elapsed >= policy_.min_seconds_between_issuance;
}

void HybridCoordinator::record_issuance(const HybridIssuanceRecord& record) {
    // Update history
    issuance_history_[record.node_id].push_back(record);
    
    // Update last issuance time
    last_issuance_time_[record.node_id] = record.issued_at;
    
    // Update epoch counts
    auto key = std::make_pair(record.node_id, record.epoch);
    epoch_key_counts_[key] += record.key_count;
    
    // Update statistics
    total_keys_issued_ += record.key_count;
    switch (record.method) {
        case KeyIssuanceMethod::POW_ONLY:
            pow_issuances_++;
            break;
        case KeyIssuanceMethod::POSTAKE_ONLY:
            postake_issuances_++;
            break;
        case KeyIssuanceMethod::HYBRID:
            hybrid_issuances_++;
            break;
    }
    
    spdlog::info("Issued {} keys to {} via {} (epoch {})",
        record.key_count,
        record.node_id.to_string().substr(0, 8),
        record.method == KeyIssuanceMethod::POW_ONLY ? "PoW" :
        record.method == KeyIssuanceMethod::POSTAKE_ONLY ? "PoStake" : "Hybrid",
        record.epoch);
}

bool HybridCoordinator::can_issue_keys(
    const NodeID& node_id,
    uint32_t key_count,
    uint64_t epoch
) const {
    // Check epoch limit
    auto key = std::make_pair(node_id, epoch);
    auto it = epoch_key_counts_.find(key);
    uint32_t current_count = (it != epoch_key_counts_.end()) ? it->second : 0;
    
    if (current_count + key_count > policy_.max_keys_per_epoch) {
        spdlog::warn("Node {} would exceed max keys per epoch ({} + {} > {})",
            node_id.to_string().substr(0, 8),
            current_count, key_count, policy_.max_keys_per_epoch);
        return false;
    }
    
    // Check rate limit
    if (!check_rate_limit(node_id)) {
        spdlog::warn("Node {} rate limited (too soon since last issuance)",
            node_id.to_string().substr(0, 8));
        return false;
    }
    
    return true;
}

std::optional<HybridIssuanceRecord> HybridCoordinator::request_keys_via_pow(
    const NodeID& node_id,
    const core::PowSolution& pow_solution,
    core::KeyType key_type,
    uint32_t key_count
) {
    uint64_t epoch = current_epoch();
    
    // Check if issuance is allowed
    if (!can_issue_keys(node_id, key_count, epoch)) {
        return std::nullopt;
    }
    
    // Verify PoW solution
    // TODO: Implement PoW verification via pow_engine_
    // For now, assume valid
    
    // Create issuance record
    HybridIssuanceRecord record;
    record.node_id = node_id;
    record.method = KeyIssuanceMethod::POW_ONLY;
    record.pow_solution = pow_solution;
    record.key_type = key_type;
    record.key_count = key_count;
    record.issued_at = current_timestamp();
    record.epoch = epoch;
    record.record_hash = record.compute_hash();
    
    // Record the issuance
    record_issuance(record);
    
    return record;
}

std::optional<HybridIssuanceRecord> HybridCoordinator::request_keys_via_postake(
    const NodeID& node_id,
    core::KeyType key_type,
    uint32_t key_count
) {
    uint64_t epoch = current_epoch();
    
    // Check if PoStake-only is allowed
    if (!policy_.allow_postake_only) {
        spdlog::warn("PoStake-only issuance not allowed by policy");
        return std::nullopt;
    }
    
    // Check if node is new (new nodes must use PoW)
    if (policy_.require_pow_for_new_nodes && is_new_node(node_id)) {
        spdlog::warn("New node {} must use PoW to bootstrap",
            node_id.to_string().substr(0, 8));
        return std::nullopt;
    }
    
    // Check if issuance is allowed
    if (!can_issue_keys(node_id, key_count, epoch)) {
        return std::nullopt;
    }
    
    // Get contribution score
    auto score = postake_engine_.calculate_score(node_id);
    if (score.total_score < policy_.min_contribution_score) {
        spdlog::warn("Node {} has insufficient contribution score ({} < {})",
            node_id.to_string().substr(0, 8),
            score.total_score,
            policy_.min_contribution_score);
        return std::nullopt;
    }
    
    // Create issuance record
    HybridIssuanceRecord record;
    record.node_id = node_id;
    record.method = KeyIssuanceMethod::POSTAKE_ONLY;
    record.contribution_score = score;
    record.key_type = key_type;
    record.key_count = key_count;
    record.issued_at = current_timestamp();
    record.epoch = epoch;
    record.record_hash = record.compute_hash();
    
    // Record the issuance
    record_issuance(record);
    
    return record;
}

std::optional<HybridIssuanceRecord> HybridCoordinator::request_keys_hybrid(
    const NodeID& node_id,
    const core::PowSolution& pow_solution,
    core::KeyType key_type,
    uint32_t key_count
) {
    uint64_t epoch = current_epoch();
    
    // Apply hybrid bonus
    uint32_t bonus_keys = calculate_hybrid_bonus(key_count);
    
    // Check if issuance is allowed (with bonus)
    if (!can_issue_keys(node_id, bonus_keys, epoch)) {
        return std::nullopt;
    }
    
    // Verify PoW solution
    // TODO: Implement PoW verification
    
    // Get contribution score
    auto score = postake_engine_.calculate_score(node_id);
    
    // Create issuance record
    HybridIssuanceRecord record;
    record.node_id = node_id;
    record.method = KeyIssuanceMethod::HYBRID;
    record.pow_solution = pow_solution;
    record.contribution_score = score;
    record.key_type = key_type;
    record.key_count = bonus_keys;  // Use bonus amount
    record.issued_at = current_timestamp();
    record.epoch = epoch;
    record.record_hash = record.compute_hash();
    
    // Record the issuance
    record_issuance(record);
    
    spdlog::info("Hybrid issuance bonus: {} keys -> {} keys ({}x multiplier)",
        key_count, bonus_keys, policy_.hybrid_multiplier);
    
    return record;
}

bool HybridCoordinator::validate_issuance(const HybridIssuanceRecord& record) const {
    // Verify hash
    auto computed_hash = record.compute_hash();
    if (computed_hash != record.record_hash) {
        spdlog::error("Issuance record hash mismatch");
        return false;
    }
    
    // Verify method-specific requirements
    switch (record.method) {
        case KeyIssuanceMethod::POW_ONLY:
            if (!record.pow_solution.has_value()) {
                spdlog::error("PoW-only issuance missing PoW solution");
                return false;
            }
            // TODO: Verify PoW solution
            break;
            
        case KeyIssuanceMethod::POSTAKE_ONLY:
            if (!record.contribution_score.has_value()) {
                spdlog::error("PoStake-only issuance missing contribution score");
                return false;
            }
            if (record.contribution_score->total_score < policy_.min_contribution_score) {
                spdlog::error("PoStake-only issuance has insufficient score");
                return false;
            }
            break;
            
        case KeyIssuanceMethod::HYBRID:
            if (!record.pow_solution.has_value() || !record.contribution_score.has_value()) {
                spdlog::error("Hybrid issuance missing PoW or PoStake data");
                return false;
            }
            // TODO: Verify PoW solution
            break;
    }
    
    return true;
}

std::vector<HybridIssuanceRecord> HybridCoordinator::get_issuance_history(
    const NodeID& node_id
) const {
    auto it = issuance_history_.find(node_id);
    if (it == issuance_history_.end()) {
        return {};
    }
    return it->second;
}

uint32_t HybridCoordinator::get_keys_issued_in_epoch(
    const NodeID& node_id,
    uint64_t epoch
) const {
    auto key = std::make_pair(node_id, epoch);
    auto it = epoch_key_counts_.find(key);
    return (it != epoch_key_counts_.end()) ? it->second : 0;
}

std::optional<uint64_t> HybridCoordinator::get_last_issuance_time(
    const NodeID& node_id
) const {
    auto it = last_issuance_time_.find(node_id);
    if (it == last_issuance_time_.end()) {
        return std::nullopt;
    }
    return it->second;
}

KeyIssuanceMethod HybridCoordinator::recommend_method(const NodeID& node_id) const {
    // New nodes should use PoW
    if (is_new_node(node_id)) {
        return KeyIssuanceMethod::POW_ONLY;
    }
    
    // Check contribution score
    auto score = postake_engine_.calculate_score(node_id);
    
    // If score is high enough and PoStake-only is allowed, recommend it
    if (policy_.allow_postake_only && 
        score.total_score >= policy_.min_contribution_score) {
        
        // If hybrid bonus is enabled, recommend hybrid
        if (policy_.enable_hybrid_bonus) {
            return KeyIssuanceMethod::HYBRID;
        }
        
        return KeyIssuanceMethod::POSTAKE_ONLY;
    }
    
    // Otherwise, recommend PoW
    return KeyIssuanceMethod::POW_ONLY;
}

} // namespace cashew::postake
