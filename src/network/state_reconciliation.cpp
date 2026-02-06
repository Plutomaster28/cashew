#include "network/state_reconciliation.hpp"
#include "utils/logger.hpp"
#include "crypto/blake3.hpp"
#include <algorithm>
#include <chrono>

namespace cashew::network {

StateReconciliation::StateReconciliation(
    ledger::Ledger& ledger,
    ledger::StateManager& state_manager
)
    : ledger_(ledger), state_manager_(state_manager)
{
}

std::optional<StateConflict> StateReconciliation::detect_conflict(
    const NodeID& peer_id,
    uint64_t epoch,
    const Hash256& peer_hash,
    const std::vector<ledger::LedgerEvent>& peer_events
) {
    // Get our state for this epoch
    Hash256 local_hash = ledger_.get_latest_hash();
    uint64_t local_epoch = ledger_.current_epoch();
    
    // Check if epochs match
    if (local_epoch != epoch) {
        CASHEW_LOG_DEBUG("Epoch mismatch: local={}, peer={}", local_epoch, epoch);
        return std::nullopt;  // Not a conflict, just different sync points
    }
    
    // Check if hashes match
    if (local_hash == peer_hash) {
        return std::nullopt;  // No conflict
    }
    
    // Conflict detected!
    conflicts_detected_++;
    
    StateConflict conflict;
    conflict.type = ConflictType::HASH_MISMATCH;
    conflict.epoch = epoch;
    conflict.local_hash = local_hash;
    conflict.remote_hash = peer_hash;
    conflict.peer_id = peer_id;
    conflict.detected_at = current_timestamp();
    
    // Get our events for comparison
    // Note: This is a simplified version - real implementation would query ledger
    conflict.local_events = {}; // Would fetch from ledger
    conflict.remote_events = peer_events;
    
    // Classify the specific type of conflict
    conflict.type = classify_conflict(
        local_hash, peer_hash,
        conflict.local_events, conflict.remote_events
    );
    
    conflict.description = "Ledger state divergence detected";
    
    CASHEW_LOG_WARN("State conflict detected with peer {} at epoch {}: {}",
                   cashew::hash_to_hex(peer_id.id).substr(0, 16), epoch, conflict.description);
    
    return conflict;
}

std::vector<StateConflict> StateReconciliation::detect_all_conflicts(
    const std::map<NodeID, std::pair<Hash256, std::vector<ledger::LedgerEvent>>>& peer_states
) {
    std::vector<StateConflict> conflicts;
    
    uint64_t our_epoch = ledger_.current_epoch();
    
    for (const auto& [peer_id, state_data] : peer_states) {
        const auto& [peer_hash, peer_events] = state_data;
        
        auto conflict = detect_conflict(peer_id, our_epoch, peer_hash, peer_events);
        if (conflict) {
            conflicts.push_back(*conflict);
        }
    }
    
    return conflicts;
}

MergeResult StateReconciliation::resolve_conflict(
    const StateConflict& conflict,
    MergeStrategy strategy
) {
    MergeResult result;
    result.strategy_used = strategy;
    result.success = false;
    
    try {
        switch (strategy) {
            case MergeStrategy::PREFER_LOCAL:
                result.success = apply_local_preference(conflict);
                break;
                
            case MergeStrategy::PREFER_REMOTE:
                result.success = apply_remote_preference(conflict);
                break;
                
            case MergeStrategy::MERGE_BOTH:
                result.success = apply_merge_both(conflict);
                break;
                
            case MergeStrategy::HIGHEST_WORK:
                result.success = apply_highest_work(conflict);
                break;
                
            case MergeStrategy::QUORUM_CONSENSUS:
                result.error_message = "Quorum consensus requires multiple peers";
                result.success = false;
                break;
                
            case MergeStrategy::MANUAL_REVIEW:
                result.error_message = "Manual review required";
                result.success = false;
                result.unresolved_conflicts.push_back(conflict);
                break;
        }
        
        if (result.success) {
            conflicts_resolved_++;
            result.conflicts_resolved = 1;
            result.final_hash = ledger_.get_latest_hash();
            result.final_epoch = ledger_.current_epoch();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Resolution failed: ") + e.what();
        CASHEW_LOG_ERROR("Conflict resolution failed: {}", e.what());
    }
    
    return result;
}

MergeResult StateReconciliation::resolve_all_conflicts(
    const std::vector<StateConflict>& conflicts
) {
    MergeResult combined_result;
    combined_result.success = true;
    combined_result.conflicts_resolved = 0;
    
    for (const auto& conflict : conflicts) {
        MergeStrategy strategy = choose_strategy(conflict);
        MergeResult result = resolve_conflict(conflict, strategy);
        
        combined_result.events_added += result.events_added;
        combined_result.events_removed += result.events_removed;
        combined_result.conflicts_resolved += result.conflicts_resolved;
        
        if (!result.success) {
            combined_result.success = false;
            combined_result.unresolved_conflicts.push_back(conflict);
        }
    }
    
    if (combined_result.success) {
        combined_result.final_hash = ledger_.get_latest_hash();
        combined_result.final_epoch = ledger_.current_epoch();
    }
    
    return combined_result;
}

MergeResult StateReconciliation::auto_reconcile_with_peer(
    const NodeID& peer_id,
    const std::vector<ledger::LedgerEvent>& peer_events
) {
    reconciliations_++;
    
    Hash256 peer_hash = compute_state_hash(peer_events, ledger_.current_epoch());
    
    auto conflict = detect_conflict(peer_id, ledger_.current_epoch(), peer_hash, peer_events);
    
    if (!conflict) {
        // No conflict, states are consistent
        MergeResult result;
        result.success = true;
        result.strategy_used = MergeStrategy::PREFER_LOCAL;
        result.final_hash = ledger_.get_latest_hash();
        result.final_epoch = ledger_.current_epoch();
        return result;
    }
    
    // Auto-choose strategy
    MergeStrategy strategy = choose_strategy(*conflict);
    return resolve_conflict(*conflict, strategy);
}

MergeResult StateReconciliation::reconcile_with_quorum(
    const std::map<NodeID, std::vector<ledger::LedgerEvent>>& peer_states
) {
    // Count how many peers agree on each state hash
    std::map<Hash256, uint32_t> hash_votes;
    std::map<Hash256, std::vector<ledger::LedgerEvent>> hash_to_events;
    
    uint64_t current_epoch = ledger_.current_epoch();
    
    for (const auto& [peer_id, events] : peer_states) {
        Hash256 hash = compute_state_hash(events, current_epoch);
        hash_votes[hash]++;
        
        if (hash_to_events.find(hash) == hash_to_events.end()) {
            hash_to_events[hash] = events;
        }
    }
    
    // Find the most popular hash (quorum consensus)
    Hash256 quorum_hash;
    uint32_t max_votes = 0;
    
    for (const auto& [hash, votes] : hash_votes) {
        if (votes > max_votes) {
            max_votes = votes;
            quorum_hash = hash;
        }
    }
    
    // Check if quorum is reached (>50% of peers)
    if (max_votes < peer_states.size() / 2) {
        MergeResult result;
        result.success = false;
        result.error_message = "No quorum consensus reached";
        return result;
    }
    
    // Check if our hash matches quorum
    Hash256 our_hash = ledger_.get_latest_hash();
    
    if (our_hash == quorum_hash) {
        // We're already in sync with quorum
        MergeResult result;
        result.success = true;
        result.strategy_used = MergeStrategy::QUORUM_CONSENSUS;
        result.events_added = 0;
        result.events_removed = 0;
        result.conflicts_resolved = 0;
        result.final_hash = our_hash;
        result.final_epoch = current_epoch;
        return result;
    }
    
    // We need to adopt the quorum state
    StateConflict conflict;
    conflict.type = ConflictType::HASH_MISMATCH;
    conflict.epoch = current_epoch;
    conflict.local_hash = our_hash;
    conflict.remote_hash = quorum_hash;
    conflict.remote_events = hash_to_events[quorum_hash];
    conflict.detected_at = current_timestamp();
    conflict.description = "Local state differs from quorum consensus";
    
    return resolve_conflict(conflict, MergeStrategy::PREFER_REMOTE);
}

bool StateReconciliation::verify_consistency(const Hash256& expected_hash, uint64_t epoch) {
    if (ledger_.current_epoch() != epoch) {
        return false;
    }
    
    Hash256 actual_hash = ledger_.get_latest_hash();
    return actual_hash == expected_hash;
}

std::vector<ledger::LedgerEvent> StateReconciliation::find_missing_events(
    const std::vector<ledger::LedgerEvent>& remote_events
) {
    std::vector<ledger::LedgerEvent> missing;
    
    // Simple implementation: check which remote events we don't have
    // Real implementation would query ledger's event log
    
    for (const auto& remote_event : remote_events) {
        // Check if we have this event (by hash or ID)
        // This is a stub - real implementation would check ledger
        bool have_event = false; // Would query ledger
        
        if (!have_event) {
            missing.push_back(remote_event);
        }
    }
    
    return missing;
}

// Private helpers

ConflictType StateReconciliation::classify_conflict(
    const Hash256& local_hash,
    const Hash256& remote_hash,
    const std::vector<ledger::LedgerEvent>& local_events,
    const std::vector<ledger::LedgerEvent>& remote_events
) {
    if (local_hash == remote_hash) {
        return ConflictType::HASH_MISMATCH; // Shouldn't happen
    }
    
    if (local_events.size() != remote_events.size()) {
        return ConflictType::MISSING_EVENTS;
    }
    
    // Check for timestamp anomalies
    uint64_t now = current_timestamp();
    for (const auto& event : remote_events) {
        if (event.timestamp > now + 300) { // More than 5 minutes in future
            return ConflictType::TIMESTAMP_ANOMALY;
        }
    }
    
    // Default to hash mismatch
    return ConflictType::HASH_MISMATCH;
}

MergeStrategy StateReconciliation::choose_strategy(const StateConflict& conflict) {
    switch (conflict.type) {
        case ConflictType::HASH_MISMATCH:
            // Use highest PoW to break ties
            return MergeStrategy::HIGHEST_WORK;
            
        case ConflictType::MISSING_EVENTS:
            // Merge both event sets
            return MergeStrategy::MERGE_BOTH;
            
        case ConflictType::TIMESTAMP_ANOMALY:
            // Prefer local if remote has bad timestamps
            return MergeStrategy::PREFER_LOCAL;
            
        case ConflictType::EPOCH_FORK:
            // Needs manual review for serious forks
            return MergeStrategy::MANUAL_REVIEW;
            
        default:
            return MergeStrategy::HIGHEST_WORK;
    }
}

bool StateReconciliation::apply_local_preference(const StateConflict& /*conflict*/) {
    // Do nothing - keep our state
    CASHEW_LOG_INFO("Keeping local state (preferred)");
    return true;
}

bool StateReconciliation::apply_remote_preference(const StateConflict& /*conflict*/) {
    // Replace our state with remote
    // This is a stub - real implementation would update ledger
    CASHEW_LOG_WARN("Adopting remote state (may lose local events)");
    
    // Would call ledger_.reset_to_state(conflict.remote_events);
    return true; // Stub
}

bool StateReconciliation::apply_merge_both(const StateConflict& conflict) {
    // Merge both event sets
    CASHEW_LOG_INFO("Merging both local and remote events");
    
    // Find missing events from remote
    auto missing = find_missing_events(conflict.remote_events);
    
    // Add missing events to our ledger
    // Would call ledger_.append_events(missing);
    
    return true; // Stub
}

bool StateReconciliation::apply_highest_work(const StateConflict& conflict) {
    uint32_t local_work = calculate_proof_of_work(conflict.local_events);
    uint32_t remote_work = calculate_proof_of_work(conflict.remote_events);
    
    CASHEW_LOG_INFO("PoW comparison: local={}, remote={}", local_work, remote_work);
    
    if (remote_work > local_work) {
        return apply_remote_preference(conflict);
    } else {
        return apply_local_preference(conflict);
    }
}

uint32_t StateReconciliation::calculate_proof_of_work(
    const std::vector<ledger::LedgerEvent>& events
) {
    uint32_t total_work = 0;
    
    for (const auto& event : events) {
        // Count PoW solutions in events
        if (event.event_type == ledger::EventType::KEY_ISSUED) {
            total_work += 1;  // Each key issuance represents PoW
        }
    }
    
    return total_work;
}

Hash256 StateReconciliation::compute_state_hash(
    const std::vector<ledger::LedgerEvent>& events,
    uint64_t epoch
) {
    // Simplified hash computation
    std::vector<uint8_t> data;
    
    // Add epoch
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(epoch >> (i * 8)));
    }
    
    // Add events
    for (const auto& event : events) {
        auto event_bytes = event.to_bytes();
        data.insert(data.end(), event_bytes.begin(), event_bytes.end());
    }
    
    return crypto::Blake3::hash(data);
}

uint64_t StateReconciliation::current_timestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace cashew::network
