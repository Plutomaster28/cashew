#pragma once

#include "cashew/common.hpp"
#include "network/session.hpp"
#include "network/gossip.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <chrono>
#include <functional>

namespace cashew::network {

// Forward declarations
class Router;

/**
 * PeerInfo - Information about a discovered peer
 */
struct PeerInfo {
    NodeID node_id;
    std::string address;  // IP:port or hostname:port
    uint64_t last_seen;
    uint64_t first_seen;
    NodeCapabilities capabilities;
    uint32_t connection_attempts;
    uint32_t successful_connections;
    bool is_bootstrap;
    
    PeerInfo()
        : last_seen(0), first_seen(0), 
          connection_attempts(0), successful_connections(0),
          is_bootstrap(false) {}
    
    float reliability_score() const;
    bool is_stale() const;
};

/**
 * PeerConnection - Active connection to a peer
 */
struct PeerConnection {
    NodeID peer_id;
    std::string address;
    Session* session;  // Owned by SessionManager
    uint64_t connected_at;
    uint64_t last_activity;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    bool is_outbound;  // We initiated (outbound-only policy)
    
    PeerConnection()
        : session(nullptr), connected_at(0), last_activity(0),
          bytes_sent(0), bytes_received(0), is_outbound(true) {}
    
    bool is_idle() const;
    std::chrono::seconds idle_duration() const;
};

/**
 * ConnectionPolicy - Policy for peer connections
 */
struct ConnectionPolicy {
    size_t max_peers = 50;
    size_t target_peers = 20;
    size_t min_peers = 5;
    
    size_t max_bootstrap_peers = 10;
    
    bool outbound_only = true;  // Cashew policy: only outbound connections
    
    uint32_t connection_timeout_seconds = 30;
    uint32_t idle_timeout_seconds = 300;  // 5 minutes
    uint32_t reconnect_delay_seconds = 60;
    
    uint32_t max_connection_attempts = 5;
    float min_reliability_score = 0.3f;
    
    bool allow_local_peers = false;  // For testing
};

/**
 * BootstrapNode - Hardcoded bootstrap node
 */
struct BootstrapNode {
    std::string address;  // IP:port or hostname:port
    PublicKey public_key;
    std::string description;
    
    BootstrapNode() = default;
    BootstrapNode(const std::string& addr, const PublicKey& key, const std::string& desc)
        : address(addr), public_key(key), description(desc) {}
};

/**
 * PeerAnnouncementMessage - Announce this node to the network
 */
struct PeerAnnouncementMessage {
    NodeID node_id;
    std::string listen_address;  // Empty for outbound-only nodes
    NodeCapabilities capabilities;
    uint64_t timestamp;
    Signature signature;  // Signed by node_id
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<PeerAnnouncementMessage> from_bytes(const std::vector<uint8_t>& data);
    bool verify_signature(const PublicKey& public_key) const;
};

/**
 * PeerRequestMessage - Request list of known peers
 */
struct PeerRequestMessage {
    NodeID requester_id;
    uint32_t max_peers;  // Maximum peers to return
    uint64_t timestamp;
    Signature signature;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<PeerRequestMessage> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * PeerResponseMessage - Response with list of known peers
 */
struct PeerResponseMessage {
    struct PeerEntry {
        NodeID node_id;
        std::string address;
        NodeCapabilities capabilities;
        uint64_t last_seen;
    };
    
    NodeID responder_id;
    std::vector<PeerEntry> peers;
    uint64_t timestamp;
    Signature signature;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<PeerResponseMessage> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * NATTraversalRequest - STUN-like NAT traversal
 */
struct NATTraversalRequest {
    NodeID requester_id;
    uint64_t nonce;  // Random nonce for this request
    uint64_t timestamp;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<NATTraversalRequest> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * NATTraversalResponse - Response with public address info
 */
struct NATTraversalResponse {
    std::string public_address;  // What requester's address looks like from our perspective
    uint16_t public_port;
    uint64_t timestamp;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<NATTraversalResponse> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * PeerDiversity - Tracks peer diversity for resilience
 */
struct PeerDiversity {
    std::set<std::string> subnets;  // /24 subnets represented
    std::set<std::string> unique_addresses;  // Unique IP addresses
    size_t estimated_geographic_regions;  // Estimated number of geographic regions
    
    PeerDiversity() : estimated_geographic_regions(0) {}
    
    bool is_diverse() const;
};

/**
 * PeerDiscovery - Mechanism for finding new peers
 */
class PeerDiscovery {
public:
    PeerDiscovery() = default;
    ~PeerDiscovery() = default;
    
    // Bootstrap peers
    void add_bootstrap_node(const BootstrapNode& node);
    std::vector<BootstrapNode> get_bootstrap_nodes() const;
    
    // Discovered peers
    void add_discovered_peer(const NodeID& node_id, const std::string& address);
    void update_peer_seen(const NodeID& node_id);
    void update_peer_capabilities(const NodeID& node_id, const NodeCapabilities& caps);
    
    std::vector<PeerInfo> get_discovered_peers() const;
    std::optional<PeerInfo> get_peer_info(const NodeID& node_id) const;
    
    // Selection
    std::vector<NodeID> select_peers_to_connect(
        size_t count,
        const std::set<NodeID>& exclude
    ) const;
    
    std::vector<NodeID> select_random_peers(
        size_t count,
        const std::set<NodeID>& exclude
    ) const;
    
    std::vector<NodeID> select_diverse_peers(
        size_t count,
        const std::set<NodeID>& exclude,
        const PeerDiversity& current_diversity
    ) const;
    
    // Diversity
    PeerDiversity calculate_diversity(const std::vector<NodeID>& connected_peers) const;
    
    // Peer database persistence
    bool save_to_disk(const std::string& filepath) const;
    bool load_from_disk(const std::string& filepath);
    
    // Cleanup
    void cleanup_stale_peers();
    size_t peer_count() const { return discovered_peers_.size(); }

private:
    std::vector<BootstrapNode> bootstrap_nodes_;
    std::map<NodeID, PeerInfo> discovered_peers_;
    
    static constexpr uint64_t PEER_STALE_TIMEOUT = 3600;  // 1 hour
    
    std::string extract_subnet(const std::string& address) const;
};

/**
 * PeerManager - Manages peer connections and discovery
 * 
 * Key features:
 * - Outbound-only connections (Cashew security model)
 * - Dynamic peer selection
 * - Connection pooling
 * - Bootstrap peer integration
 * - Gossip integration for peer discovery
 * - Session lifecycle management
 */
class PeerManager {
public:
    PeerManager(
        const NodeID& local_node_id,
        SessionManager& session_manager,
        GossipProtocol& gossip_protocol
    );
    ~PeerManager();
    
    // Configuration
    void set_policy(const ConnectionPolicy& policy) { policy_ = policy; }
    const ConnectionPolicy& get_policy() const { return policy_; }
    
    // Bootstrap
    void add_bootstrap_node(const BootstrapNode& node);
    void connect_to_bootstrap_nodes();
    
    // Peer discovery
    void add_discovered_peer(const NodeID& node_id, const std::string& address);
    void update_peer_capabilities(const NodeID& node_id, const NodeCapabilities& caps);
    
    // Connection management
    void connect_to_peer(const NodeID& peer_id, const std::string& address);
    void disconnect_peer(const NodeID& peer_id);
    void disconnect_all();
    
    bool is_connected(const NodeID& peer_id) const;
    std::optional<PeerConnection> get_connection(const NodeID& peer_id) const;
    std::vector<PeerConnection> get_all_connections() const;
    
    size_t active_connection_count() const { return active_connections_.size(); }
    size_t discovered_peer_count() const { return discovery_.peer_count(); }
    
    // Automatic peer management
    void maintain_peer_connections();  // Call periodically
    void cleanup_idle_connections();
    void attempt_reconnections();
    
    // Sending messages (integrates with Router/Gossip)
    bool send_to_peer(const NodeID& peer_id, const std::vector<uint8_t>& data);
    void broadcast_to_peers(const std::vector<uint8_t>& data);
    void send_to_random_peers(const std::vector<uint8_t>& data, size_t count);
    
    // Callbacks
    using PeerConnectedCallback = std::function<void(const NodeID&)>;
    using PeerDisconnectedCallback = std::function<void(const NodeID&)>;
    using MessageReceivedCallback = std::function<void(const NodeID&, const std::vector<uint8_t>&)>;
    
    void set_peer_connected_callback(PeerConnectedCallback callback) {
        peer_connected_callback_ = callback;
    }
    
    void set_peer_disconnected_callback(PeerDisconnectedCallback callback) {
        peer_disconnected_callback_ = callback;
    }
    
    void set_message_received_callback(MessageReceivedCallback callback) {
        message_received_callback_ = callback;
    }
    
    // Statistics
    uint64_t total_connections_made() const { return total_connections_made_; }
    uint64_t total_connection_failures() const { return total_connection_failures_; }
    uint64_t total_bytes_sent() const;
    uint64_t total_bytes_received() const;
    
    // Message handlers
    void handle_peer_announcement(const PeerAnnouncementMessage& msg);
    void handle_peer_request(const NodeID& requesting_peer, const PeerRequestMessage& msg);
    void handle_peer_response(const PeerResponseMessage& msg);
    void handle_nat_traversal_request(const NodeID& requesting_peer, const NATTraversalRequest& req);
    
    // Peer database
    bool save_peer_database(const std::string& filepath) const;
    bool load_peer_database(const std::string& filepath);

private:
    NodeID local_node_id_;
    SessionManager& session_manager_;
    GossipProtocol& gossip_protocol_;
    
    ConnectionPolicy policy_;
    PeerDiscovery discovery_;
    
    // Active connections
    std::map<NodeID, PeerConnection> active_connections_;
    
    // Connection tracking
    std::map<NodeID, uint64_t> last_connection_attempt_;
    std::map<NodeID, uint32_t> connection_failure_count_;
    
    // Callbacks
    PeerConnectedCallback peer_connected_callback_;
    PeerDisconnectedCallback peer_disconnected_callback_;
    MessageReceivedCallback message_received_callback_;
    
    // Statistics
    uint64_t total_connections_made_;
    uint64_t total_connection_failures_;
    
    // Helpers
    bool should_connect_to_peer(const NodeID& peer_id) const;
    bool can_connect_more_peers() const;
    void handle_successful_connection(const NodeID& peer_id, Session* session);
    void handle_failed_connection(const NodeID& peer_id);
    void cleanup_connection(const NodeID& peer_id);
    
    uint64_t current_timestamp() const;
};

/**
 * PeerStatistics - Detailed peer statistics
 */
struct PeerStatistics {
    size_t active_connections;
    size_t discovered_peers;
    size_t bootstrap_nodes;
    
    uint64_t total_connections_made;
    uint64_t total_connection_failures;
    float average_connection_success_rate;
    
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    
    std::chrono::seconds average_connection_duration;
    
    std::string to_string() const;
};

} // namespace cashew::network
