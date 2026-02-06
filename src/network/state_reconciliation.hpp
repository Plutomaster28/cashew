#pragma once

#include "cashew/common.hpp"
#include "core/ledger/ledger.hpp"
#include "core/ledger/state.hpp"
#include <vector>
#include <map>
#include <optional>
#include <functional>

namespace cashew::network {

/**
 * ConflictType - Types of state conflicts
 */
enum class ConflictType {
    HASH_MISMATCH,      // Same epoch, different hashes
    EPOCH_FORK,         // Divergent epoch sequences
    MISSING_EVENTS,     // One side has events other doesn't
    DUPLICATE_EVENTS,   // Same events with different signatures
    TIMESTAMP_ANOMALY   // Events with impossible timestamps
};

/**
 * StateConflict - Represents a conflict between local and remote state
 */
struct StateConflict {
    ConflictType type;
    uint64_t epoch;
    
    Hash256 local_hash;
    Hash256 remote_hash;
    
    std::vector<ledger::LedgerEvent> local_events;
    std::vector<ledger::LedgerEvent> remote_events;
    
    NodeID peer_id;
    uint64_t detected_at;
    
    std::string description;
};

/**
 * MergeStrategy - How to resolve conflicts
 */
enum class MergeStrategy {
    PREFER_LOCAL,       // Keep our state
    PREFER_REMOTE,      // Accept their state
    MERGE_BOTH,         // Merge both event sets
    HIGHEST_WORK,       // Choose state with most PoW
    QUORUM_CONSENSUS,   // Choose state most peers agree on
    MANUAL_REVIEW       // Flag for manual intervention
};

/**
 * MergeResult - Result of merging conflicting states
 */
struct MergeResult {
    bool success;
    MergeStrategy strategy_used;
    
    uint64_t events_added;
    uint64_t events_removed;
    uint64_t conflicts_resolved;
    
    Hash256 final_hash;
    uint64_t final_epoch;
    
    std::string error_message;
    std::vector<StateConflict> unresolved_conflicts;
};

/**
 * StateReconciliation - Reconciles divergent ledger states
 * 
 * Handles conflicts when peers have different views of the ledger:
 * - Detects hash mismatches
 * - Identifies missing/divergent events
 * - Applies merge strategies
 * - Ensures eventual consistency
 */
class StateReconciliation {
public:
    StateReconciliation(ledger::Ledger& ledger, ledger::StateManager& state_manager);
    ~StateReconciliation() = default;
    
    // Conflict detection
    std::optional<StateConflict> detect_conflict(
        const NodeID& peer_id,
        uint64_t epoch,
        const Hash256& peer_hash,
        const std::vector<ledger::LedgerEvent>& peer_events
    );
    
    std::vector<StateConflict> detect_all_conflicts(
        const std::map<NodeID, std::pair<Hash256, std::vector<ledger::LedgerEvent>>>& peer_states
    );
    
    // Resolution
    MergeResult resolve_conflict(const StateConflict& conflict, MergeStrategy strategy);
    MergeResult resolve_all_conflicts(const std::vector<StateConflict>& conflicts);
    
    // Automatic reconciliation
    MergeResult auto_reconcile_with_peer(
        const NodeID& peer_id,
        const std::vector<ledger::LedgerEvent>& peer_events
    );
    
    // Quorum-based consensus
    MergeResult reconcile_with_quorum(
        const std::map<NodeID, std::vector<ledger::LedgerEvent>>& peer_states
    );
    
    // State comparison
    bool verify_consistency(const Hash256& expected_hash, uint64_t epoch);
    std::vector<ledger::LedgerEvent> find_missing_events(
        const std::vector<ledger::LedgerEvent>& remote_events
    );
    
    // Statistics
    uint64_t get_conflicts_detected() const { return conflicts_detected_; }
    uint64_t get_conflicts_resolved() const { return conflicts_resolved_; }
    uint64_t get_reconciliations_performed() const { return reconciliations_; }
    
private:
    ledger::Ledger& ledger_;
    ledger::StateManager& state_manager_;
    
    // Statistics
    uint64_t conflicts_detected_ = 0;
    uint64_t conflicts_resolved_ = 0;
    uint64_t reconciliations_ = 0;
    
    // Helpers
    ConflictType classify_conflict(
        const Hash256& local_hash,
        const Hash256& remote_hash,
        const std::vector<ledger::LedgerEvent>& local_events,
        const std::vector<ledger::LedgerEvent>& remote_events
    );
    
    MergeStrategy choose_strategy(const StateConflict& conflict);
    
    bool apply_local_preference(const StateConflict& conflict);
    bool apply_remote_preference(const StateConflict& conflict);
    bool apply_merge_both(const StateConflict& conflict);
    bool apply_highest_work(const StateConflict& conflict);
    
    uint32_t calculate_proof_of_work(const std::vector<ledger::LedgerEvent>& events);
    Hash256 compute_state_hash(const std::vector<ledger::LedgerEvent>& events, uint64_t epoch);
    
    uint64_t current_timestamp() const;
};

} // namespace cashew::network
