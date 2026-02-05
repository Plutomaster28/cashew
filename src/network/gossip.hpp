#pragma once

#include "cashew/common.hpp"
#include "core/keys/key.hpp"
#include <vector>
#include <optional>
#include <set>
#include <functional>
#include <chrono>

namespace cashew::network {

/**
 * GossipMessageType - Type of gossip message
 */
enum class GossipMessageType : uint8_t {
    PEER_ANNOUNCEMENT = 1,      // Node announcing its presence
    CONTENT_ANNOUNCEMENT = 2,   // Node announcing hosted content
    NETWORK_STATE_UPDATE = 3,   // Network-wide state update
    KEY_REVOCATION = 4,         // Revoked key announcement
    NODE_CAPABILITY = 5         // Node capability advertisement
};

/**
 * NodeCapabilities - What a node can do
 */
struct NodeCapabilities {
    bool can_host_things;
    bool can_route_content;
    bool can_provide_storage;
    uint64_t storage_capacity_bytes;
    uint64_t bandwidth_capacity_mbps;
    
    NodeCapabilities()
        : can_host_things(true), can_route_content(true),
          can_provide_storage(true),
          storage_capacity_bytes(0),
          bandwidth_capacity_mbps(0) {}
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<NodeCapabilities> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * PeerAnnouncement - Node announcing itself to the network
 */
struct PeerAnnouncement {
    NodeID node_id;
    PublicKey public_key;
    NodeCapabilities capabilities;
    uint64_t timestamp;
    Signature signature;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<PeerAnnouncement> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * ContentAnnouncement - Node announcing it hosts specific content
 */
struct ContentAnnouncement {
    ContentHash content_hash;
    uint64_t content_size;
    NodeID hosting_node;
    std::optional<Hash256> network_id;  // If part of a network
    uint64_t timestamp;
    Signature signature;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<ContentAnnouncement> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * NetworkStateUpdate - Epoch-based network state
 */
struct NetworkStateUpdate {
    uint64_t epoch_number;
    std::vector<NodeID> active_nodes;
    uint32_t pow_difficulty;
    std::vector<uint8_t> entropy_seed;  // For PoW
    uint64_t timestamp;
    std::vector<Signature> signatures;  // Multi-party signed
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<NetworkStateUpdate> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * KeyRevocation - Announcement of revoked key
 */
struct KeyRevocation {
    PublicKey revoked_key;
    NodeID revoking_node;
    uint64_t timestamp;
    std::string reason;  // Optional explanation
    Signature signature;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<KeyRevocation> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * GossipMessage - Generic gossip message envelope
 */
struct GossipMessage {
    GossipMessageType type;
    Hash256 message_id;  // For deduplication
    std::vector<uint8_t> payload;
    uint64_t timestamp;
    uint8_t hop_count;   // How many times forwarded
    
    static constexpr uint8_t MAX_HOPS = 10;
    static constexpr uint64_t MAX_AGE_SECONDS = 300;  // 5 minutes
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<GossipMessage> from_bytes(const std::vector<uint8_t>& data);
    
    Hash256 compute_id() const;
    bool is_too_old() const;
    bool has_exceeded_hops() const;
};

/**
 * GossipHandler - Callback for processing gossip messages
 */
using GossipHandler = std::function<void(const GossipMessage&)>;

/**
 * GossipProtocol - Epidemic-style message propagation
 * 
 * Key features:
 * - Deduplication (seen message cache)
 * - Anti-spam (rate limiting)
 * - Selective propagation (random peer subset)
 * - Bandwidth-efficient (configurable fanout)
 */
class GossipProtocol {
public:
    GossipProtocol(const NodeID& local_node_id);
    ~GossipProtocol() = default;
    
    // Message handling
    void receive_message(const GossipMessage& message);
    void broadcast_message(const GossipMessage& message);
    
    // Message creation helpers
    GossipMessage create_peer_announcement(const NodeCapabilities& capabilities);
    GossipMessage create_content_announcement(
        const ContentHash& content_hash,
        uint64_t content_size,
        std::optional<Hash256> network_id = std::nullopt
    );
    GossipMessage create_network_state_update(const NetworkStateUpdate& state);
    GossipMessage create_key_revocation(const PublicKey& revoked_key, const std::string& reason);
    
    // Handler registration
    void register_handler(GossipMessageType type, GossipHandler handler);
    void unregister_handler(GossipMessageType type);
    
    // Peer management
    void add_peer(const NodeID& peer_id);
    void remove_peer(const NodeID& peer_id);
    std::vector<NodeID> get_random_peers(size_t count) const;
    
    // Configuration
    void set_fanout(size_t fanout) { fanout_ = fanout; }
    void set_max_seen_messages(size_t max) { max_seen_messages_ = max; }
    
    size_t get_fanout() const { return fanout_; }
    
    // Statistics
    size_t seen_message_count() const { return seen_messages_.size(); }
    size_t peer_count() const { return peers_.size(); }
    uint64_t messages_received() const { return messages_received_; }
    uint64_t messages_sent() const { return messages_sent_; }
    
    // Maintenance
    void cleanup_old_seen_messages();
    
    // For testing
    bool has_seen_message(const Hash256& message_id) const;

private:
    NodeID local_node_id_;
    
    // Peers to propagate to
    std::vector<NodeID> peers_;
    
    // Seen message deduplication
    struct SeenMessage {
        Hash256 message_id;
        uint64_t timestamp;
    };
    std::vector<SeenMessage> seen_messages_;
    
    // Message handlers
    std::vector<std::pair<GossipMessageType, GossipHandler>> handlers_;
    
    // Configuration
    size_t fanout_;  // Number of peers to forward to
    size_t max_seen_messages_;
    
    // Statistics
    uint64_t messages_received_;
    uint64_t messages_sent_;
    
    // Constants
    static constexpr size_t DEFAULT_FANOUT = 3;
    static constexpr size_t DEFAULT_MAX_SEEN = 10000;
    static constexpr uint64_t SEEN_MESSAGE_TTL_SECONDS = 600;  // 10 minutes
    
    // Internal helpers
    bool should_propagate(const GossipMessage& message);
    void mark_as_seen(const Hash256& message_id);
    void invoke_handlers(const GossipMessage& message);
    
    // TODO: Add actual network sending
    void send_to_peer(const NodeID& peer_id, const GossipMessage& message);
};

/**
 * GossipScheduler - Periodic gossip tasks
 * 
 * Manages periodic announcements and state updates:
 * - Peer announcements every 5 minutes
 * - Content announcements on change
 * - Network state every epoch (10 minutes)
 */
class GossipScheduler {
public:
    GossipScheduler(GossipProtocol& protocol);
    ~GossipScheduler() = default;
    
    // Start/stop scheduler
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Immediate announcements
    void announce_peer(const NodeCapabilities& capabilities);
    void announce_content(const ContentHash& content_hash, uint64_t content_size);
    
    // Configuration
    void set_peer_announcement_interval(std::chrono::seconds interval) {
        peer_announcement_interval_ = interval;
    }
    void set_state_update_interval(std::chrono::seconds interval) {
        state_update_interval_ = interval;
    }
    
    // For testing
    uint64_t get_last_peer_announcement_time() const { return last_peer_announcement_; }

private:
    GossipProtocol& protocol_;
    bool running_;
    
    // Intervals
    std::chrono::seconds peer_announcement_interval_;
    std::chrono::seconds state_update_interval_;
    
    // Last announcement times
    uint64_t last_peer_announcement_;
    uint64_t last_state_update_;
    
    // Default intervals
    static constexpr uint64_t DEFAULT_PEER_INTERVAL_SECONDS = 300;  // 5 minutes
    static constexpr uint64_t DEFAULT_STATE_INTERVAL_SECONDS = 600;  // 10 minutes
    
    // Background thread management
    void run_scheduler_loop();
    
    // TODO: Add thread for background scheduling
};

} // namespace cashew::network
