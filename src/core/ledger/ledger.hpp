#pragma once

#include "cashew/common.hpp"
#include "core/keys/key.hpp"
#include "core/thing/thing.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <chrono>
#include <functional>

namespace cashew::ledger {

/**
 * IssuanceMethod - How keys were issued
 */
enum class IssuanceMethod : uint8_t {
    POW = 1,         // Proof-of-Work
    POSTAKE = 2,     // Proof-of-Stake (contribution)
    VOUCHED = 3      // Social vouching
};

/**
 * EventType - Types of events tracked in the ledger
 */
enum class EventType : uint8_t {
    // Node events
    NODE_JOINED = 1,
    NODE_LEFT = 2,
    
    // Key events
    KEY_ISSUED = 10,
    KEY_TRANSFERRED = 11,
    KEY_REVOKED = 12,
    KEY_DECAYED = 13,
    
    // Network events
    NETWORK_CREATED = 20,
    NETWORK_INVITATION_SENT = 21,
    NETWORK_INVITATION_ACCEPTED = 22,
    NETWORK_MEMBER_ADDED = 23,
    NETWORK_MEMBER_REMOVED = 24,
    NETWORK_DISBANDED = 25,
    
    // Thing events
    THING_CREATED = 30,
    THING_REPLICATED = 31,
    THING_REMOVED = 32,
    
    // Reputation events
    REPUTATION_UPDATED = 40,
    ATTESTATION_CREATED = 41,
    VOUCH_CREATED = 42,
    
    // PoW/PoStake events
    POW_SOLUTION_SUBMITTED = 50,
    POSTAKE_CONTRIBUTION = 51,
    
    // Identity events
    IDENTITY_CREATED = 60,
    IDENTITY_ROTATED = 61,
    IDENTITY_REVOKED = 62,
};

/**
 * LedgerEvent - A single event in the append-only ledger
 */
struct LedgerEvent {
    Hash256 event_id;           // Unique event identifier
    EventType event_type;
    NodeID source_node;         // Node that originated this event
    uint64_t timestamp;         // Unix timestamp
    uint64_t epoch;             // Epoch number when event occurred
    Hash256 previous_hash;      // Hash of previous event (chain)
    std::vector<uint8_t> data;  // Event-specific data
    Signature signature;        // Signature by source_node
    
    LedgerEvent()
        : event_type(EventType::NODE_JOINED), timestamp(0), epoch(0) {}
    
    // Serialization
    std::vector<uint8_t> to_bytes() const;
    static std::optional<LedgerEvent> from_bytes(const std::vector<uint8_t>& bytes);
    
    // Verification
    bool verify_signature(const PublicKey& public_key) const;
    Hash256 compute_hash() const;
};

/**
 * KeyIssuanceData - Data for KEY_ISSUED events
 */
struct KeyIssuanceData {
    core::KeyType key_type;
    uint32_t count;
    IssuanceMethod method;  // PoW, PoStake, Vouched
    Hash256 proof;                // PoW solution or contribution proof
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<KeyIssuanceData> from_bytes(const std::vector<uint8_t>& bytes);
};

/**
 * NetworkMembershipData - Data for network membership events
 */
struct NetworkMembershipData {
    Hash256 network_id;
    NodeID member_node;
    std::string role;  // "FOUNDER", "FULL", "OBSERVER"
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<NetworkMembershipData> from_bytes(const std::vector<uint8_t>& bytes);
};

/**
 * ThingReplicationData - Data for Thing replication events
 */
struct ThingReplicationData {
    ContentHash content_hash;
    Hash256 network_id;
    NodeID hosting_node;
    uint64_t size_bytes;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<ThingReplicationData> from_bytes(const std::vector<uint8_t>& bytes);
};

/**
 * ReputationUpdateData - Data for reputation events
 */
struct ReputationUpdateData {
    NodeID subject_node;
    int32_t score_delta;  // Change in reputation (can be negative)
    std::string reason;
    Hash256 evidence;     // Optional proof/reference
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<ReputationUpdateData> from_bytes(const std::vector<uint8_t>& bytes);
};

/**
 * LedgerIndex - Fast lookup indices for ledger queries
 */
class LedgerIndex {
public:
    LedgerIndex() = default;
    
    // Index management
    void add_event(const LedgerEvent& event);
    void rebuild_from_events(const std::vector<LedgerEvent>& events);
    
    // Node queries
    std::vector<Hash256> get_events_by_node(const NodeID& node_id) const;
    std::optional<uint64_t> get_node_join_time(const NodeID& node_id) const;
    
    // Key queries
    std::vector<Hash256> get_key_events_by_node(const NodeID& node_id) const;
    uint32_t get_total_keys_issued(const NodeID& node_id, core::KeyType type) const;
    
    // Network queries
    std::vector<Hash256> get_network_events(const Hash256& network_id) const;
    std::vector<NodeID> get_network_members(const Hash256& network_id) const;
    
    // Thing queries
    std::vector<Hash256> get_thing_events(const ContentHash& content_hash) const;
    std::vector<NodeID> get_thing_hosts(const ContentHash& content_hash) const;
    
    // Reputation queries
    std::vector<Hash256> get_reputation_events(const NodeID& node_id) const;
    
    void clear();

private:
    // Event indices
    std::map<NodeID, std::vector<Hash256>> events_by_node_;
    std::map<EventType, std::vector<Hash256>> events_by_type_;
    std::map<Hash256, std::vector<Hash256>> events_by_network_;
    std::map<ContentHash, std::vector<Hash256>> events_by_thing_;
    
    // State caches
    std::map<NodeID, uint64_t> node_join_times_;
    std::map<Hash256, std::set<NodeID>> network_members_;
    std::map<ContentHash, std::set<NodeID>> thing_hosts_;
    std::map<std::pair<NodeID, core::KeyType>, uint32_t> key_balances_;
};

/**
 * Event callback function type
 * Called whenever a new event is added to the ledger
 */
using EventCallback = std::function<void(const LedgerEvent& event)>;

/**
 * Ledger - Append-only distributed event log
 * 
 * Key features:
 * - Cryptographically chained events
 * - Distributed via gossip protocol
 * - No blockchain (simpler append-only log)
 * - Fast query indices
 * - Conflict detection
 * - Fork detection
 */
class Ledger {
public:
    Ledger(const NodeID& local_node_id);
    ~Ledger() = default;
    
    /**
     * Set event callback for real-time notifications
     * @param callback Function to call when new events are added
     */
    void set_event_callback(EventCallback callback);
    
    // Event creation
    Hash256 record_node_joined(const NodeID& node_id);
    Hash256 record_node_left(const NodeID& node_id);
    
    Hash256 record_key_issued(
        core::KeyType key_type,
        uint32_t count,
        IssuanceMethod method,
        const Hash256& proof
    );
    
    Hash256 record_key_revoked(
        core::KeyType key_type,
        uint32_t count,
        const std::string& reason
    );
    
    Hash256 record_network_created(const Hash256& network_id);
    Hash256 record_network_member_added(
        const Hash256& network_id,
        const NodeID& member_node,
        const std::string& role
    );
    
    Hash256 record_thing_replicated(
        const ContentHash& content_hash,
        const Hash256& network_id,
        const NodeID& hosting_node,
        uint64_t size_bytes
    );
    
    Hash256 record_reputation_update(
        const NodeID& subject_node,
        int32_t score_delta,
        const std::string& reason
    );
    
    // Event retrieval
    std::optional<LedgerEvent> get_event(const Hash256& event_id) const;
    std::vector<LedgerEvent> get_events_by_node(const NodeID& node_id) const;
    std::vector<LedgerEvent> get_events_by_type(EventType type) const;
    std::vector<LedgerEvent> get_recent_events(size_t count) const;
    std::vector<LedgerEvent> get_all_events() const;
    
    // External event handling (from gossip)
    bool add_external_event(const LedgerEvent& event);
    bool verify_event_chain(const LedgerEvent& event) const;
    
    // Queries (via index)
    const LedgerIndex& get_index() const { return index_; }
    
    // Statistics
    size_t event_count() const { return events_.size(); }
    uint64_t current_epoch() const;
    Hash256 get_latest_hash() const;
    
    // Persistence
    bool save_to_file(const std::string& filepath) const;
    bool load_from_file(const std::string& filepath);
    
    // Validation
    bool validate_chain() const;
    std::vector<Hash256> detect_conflicts() const;
    
private:
    NodeID local_node_id_;
    
    // Event storage (append-only)
    std::vector<LedgerEvent> events_;
    std::map<Hash256, size_t> event_lookup_;  // event_id -> index in events_
    
    // Index for fast queries
    LedgerIndex index_;
    
    // Chain integrity
    Hash256 latest_hash_;
    uint64_t event_counter_;
    
    // Event callback for real-time notifications
    EventCallback event_callback_;
    
    // Helpers
    LedgerEvent create_event(EventType type, const std::vector<uint8_t>& data);
    Hash256 add_event(const LedgerEvent& event);
    bool verify_event(const LedgerEvent& event) const;
};

/**
 * LedgerStatistics - Summary statistics from ledger
 */
struct LedgerStatistics {
    size_t total_events;
    size_t total_nodes;
    size_t total_networks;
    size_t total_things;
    
    uint64_t total_keys_issued;
    uint64_t total_keys_revoked;
    
    uint64_t oldest_event_timestamp;
    uint64_t newest_event_timestamp;
    
    std::string to_string() const;
};

} // namespace cashew::ledger
