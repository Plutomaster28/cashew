#pragma once

#include "cashew/common.hpp"
#include "core/ledger/ledger.hpp"
#include "core/ledger/state.hpp"
#include "network/gossip.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace cashew::network {

/**
 * LedgerSyncMessage - Gossip message for ledger synchronization
 */
struct LedgerSyncMessage {
    enum class Type {
        EVENT_BROADCAST,      // New event to propagate
        SYNC_REQUEST,         // Request events from epoch range
        SYNC_RESPONSE,        // Response with requested events
        CHECKPOINT           // Periodic ledger checkpoint
    };
    
    Type type;
    std::vector<ledger::LedgerEvent> events;
    uint64_t start_epoch;
    uint64_t end_epoch;
    Hash256 ledger_hash;      // For validation
    
    std::vector<uint8_t> serialize() const;
    static std::optional<LedgerSyncMessage> deserialize(const std::vector<uint8_t>& data);
};

/**
 * SyncState - Track synchronization progress with peers
 */
struct SyncState {
    NodeID peer_id;
    uint64_t last_synced_epoch;
    Hash256 last_known_hash;
    uint64_t last_sync_time;
    bool sync_in_progress;
    
    SyncState() : last_synced_epoch(0), last_sync_time(0), sync_in_progress(false) {}
};

/**
 * LedgerGossipBridge - Connects ledger to gossip protocol
 * 
 * Responsibilities:
 * - Broadcast new ledger events via gossip
 * - Request missing events from peers
 * - Validate incoming events
 * - Maintain consistency across network
 * 
 * Design:
 * - Events gossipped immediately when created
 * - Periodic sync requests for missing epochs
 * - Checkpoint broadcast every N epochs
 * - Conflict detection and resolution
 */
class LedgerGossipBridge {
public:
    LedgerGossipBridge(ledger::Ledger& ledger, 
                      ledger::StateManager& state_manager,
                      GossipProtocol& gossip);
    ~LedgerGossipBridge() = default;
    
    // Event broadcasting
    void broadcast_event(const ledger::LedgerEvent& event);
    void broadcast_checkpoint(uint64_t epoch);
    
    // Synchronization
    void request_sync(const NodeID& peer_id, uint64_t start_epoch, uint64_t end_epoch);
    void handle_sync_request(const NodeID& peer_id, uint64_t start_epoch, uint64_t end_epoch);
    void handle_sync_response(const NodeID& peer_id, const std::vector<ledger::LedgerEvent>& events);
    
    // Message handling
    void handle_gossip_message(const NodeID& source, const GossipMessage& message);
    
    // Periodic maintenance
    void sync_with_network();      // Request missing events
    void validate_consistency();   // Check for conflicts
    void cleanup_sync_state();
    
    // Query state
    bool is_synced() const;
    uint64_t get_sync_epoch() const;
    std::vector<NodeID> get_synced_peers() const;
    
    // Statistics
    uint64_t get_events_received() const { return events_received_; }
    uint64_t get_events_sent() const { return events_sent_; }
    uint64_t get_sync_requests() const { return sync_requests_; }
    
private:
    ledger::Ledger& ledger_;
    ledger::StateManager& state_manager_;
    GossipProtocol& gossip_;
    
    // Sync tracking
    std::map<NodeID, SyncState> peer_sync_states_;
    std::set<Hash256> seen_event_ids_;  // Deduplication
    
    // Statistics
    uint64_t events_received_;
    uint64_t events_sent_;
    uint64_t sync_requests_;
    
    // Helpers
    void process_received_event(const ledger::LedgerEvent& event);
    bool validate_event_chain(const ledger::LedgerEvent& event) const;
    void update_peer_sync_state(const NodeID& peer_id, uint64_t epoch, const Hash256& hash);
    
    uint64_t current_timestamp() const;
};

/**
 * LedgerSyncScheduler - Periodic synchronization scheduler
 * 
 * Runs background tasks:
 * - Periodic sync requests (every 60 seconds)
 * - Checkpoint broadcasts (every 10 epochs)
 * - Consistency validation (every 5 minutes)
 */
class LedgerSyncScheduler {
public:
    LedgerSyncScheduler(LedgerGossipBridge& bridge);
    ~LedgerSyncScheduler() = default;
    
    void start();
    void stop();
    void tick();  // Called periodically
    
    bool is_running() const { return running_; }
    
private:
    LedgerGossipBridge& bridge_;
    bool running_;
    
    uint64_t last_sync_time_;
    uint64_t last_checkpoint_epoch_;
    uint64_t last_validation_time_;
    
    static constexpr uint64_t SYNC_INTERVAL = 60;           // 60 seconds
    static constexpr uint64_t CHECKPOINT_INTERVAL = 10;     // 10 epochs
    static constexpr uint64_t VALIDATION_INTERVAL = 300;    // 5 minutes
    
    uint64_t current_timestamp() const;
};

} // namespace cashew::network
