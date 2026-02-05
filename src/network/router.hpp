#pragma once

#include "cashew/common.hpp"
#include "core/thing/thing.hpp"
#include <vector>
#include <optional>
#include <map>
#include <chrono>
#include <functional>

namespace cashew::network {

/**
 * RoutingEntry - Information about a node and its advertised content
 */
struct RoutingEntry {
    NodeID node_id;
    std::vector<ContentHash> advertised_content;
    uint8_t hop_distance;
    uint64_t last_seen_timestamp;
    float reliability_score;  // 0.0 to 1.0
    
    RoutingEntry()
        : hop_distance(255), last_seen_timestamp(0), reliability_score(1.0f) {}
    
    RoutingEntry(const NodeID& id, uint8_t hops)
        : node_id(id), hop_distance(hops), 
          last_seen_timestamp(0), reliability_score(1.0f) {}
    
    bool is_stale() const;
};

/**
 * ContentRequest - Request to fetch content by hash
 */
struct ContentRequest {
    ContentHash content_hash;
    NodeID requester_id;
    Hash256 request_id;  // Unique request ID
    uint8_t hop_limit;
    uint64_t timestamp;
    
    // Optional onion routing layers
    std::vector<std::vector<uint8_t>> onion_layers;
    
    static constexpr uint8_t DEFAULT_HOP_LIMIT = 8;
    static constexpr uint8_t MAX_HOP_LIMIT = 16;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<ContentRequest> from_bytes(const std::vector<uint8_t>& data);
    
    Hash256 compute_id() const;
};

/**
 * ContentResponse - Response containing requested content
 */
struct ContentResponse {
    ContentHash content_hash;
    std::vector<uint8_t> content_data;
    NodeID hosting_node;
    Hash256 request_id;
    uint8_t hop_count;  // How many hops it took
    Signature signature;  // Signed by hosting node
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<ContentResponse> from_bytes(const std::vector<uint8_t>& data);
    
    bool verify_signature(const PublicKey& host_public_key) const;
};

/**
 * RouteHop - Single hop in a multi-hop route
 */
struct RouteHop {
    NodeID next_hop;
    std::vector<uint8_t> encrypted_payload;  // Encrypted next layer
};

/**
 * RoutingTable - Distributed routing table for content discovery
 */
class RoutingTable {
public:
    RoutingTable() = default;
    ~RoutingTable() = default;
    
    // Node management
    void add_node(const NodeID& node_id, uint8_t hop_distance);
    void remove_node(const NodeID& node_id);
    void update_node_seen(const NodeID& node_id);
    void update_node_reliability(const NodeID& node_id, float score);
    
    std::optional<RoutingEntry> get_entry(const NodeID& node_id) const;
    std::vector<RoutingEntry> get_all_entries() const;
    
    // Content advertisement
    void advertise_content(const NodeID& node_id, const ContentHash& content_hash);
    void remove_content_advertisement(const NodeID& node_id, const ContentHash& content_hash);
    
    // Content lookup
    std::vector<NodeID> find_hosts_for_content(const ContentHash& content_hash) const;
    bool has_content_route(const ContentHash& content_hash) const;
    
    // Best route selection
    std::optional<NodeID> select_best_host(const ContentHash& content_hash) const;
    std::vector<NodeID> select_multiple_hosts(const ContentHash& content_hash, size_t count) const;
    
    // Maintenance
    void cleanup_stale_entries();
    size_t entry_count() const { return entries_.size(); }
    size_t content_index_size() const { return content_index_.size(); }

private:
    std::map<NodeID, RoutingEntry> entries_;
    std::map<ContentHash, std::vector<NodeID>> content_index_;
    
    static constexpr uint64_t ENTRY_TTL_SECONDS = 3600;  // 1 hour
    static constexpr float MIN_RELIABILITY_SCORE = 0.3f;
};

/**
 * PendingRequest - Track in-flight content requests
 */
struct PendingRequest {
    Hash256 request_id;
    ContentHash content_hash;
    NodeID original_requester;
    uint64_t timestamp;
    uint8_t retries;
    
    static constexpr uint8_t MAX_RETRIES = 3;
    static constexpr uint64_t TIMEOUT_SECONDS = 30;
    
    bool has_timed_out() const;
};

/**
 * Router - Multi-hop content-addressed routing
 * 
 * Key features:
 * - Content-addressed routing (hash-based lookup, not IP-based)
 * - Multi-hop forwarding (up to 8-16 hops)
 * - Distributed routing table
 * - Request/response tracking
 * - Optional onion routing for privacy
 * - Local caching
 */
class Router {
public:
    Router(const NodeID& local_node_id);
    ~Router() = default;
    
    // Content requests
    Hash256 request_content(
        const ContentHash& content_hash,
        uint8_t hop_limit = ContentRequest::DEFAULT_HOP_LIMIT
    );
    
    Hash256 request_content_with_onion_routing(
        const ContentHash& content_hash,
        const std::vector<NodeID>& route_path
    );
    
    // Request handling (when we receive a request)
    void handle_content_request(const ContentRequest& request);
    void handle_content_response(const ContentResponse& response);
    
    // Routing table management
    void update_routing_table(const NodeID& node_id, uint8_t hop_distance);
    void advertise_local_content(const ContentHash& content_hash);
    void remove_local_content(const ContentHash& content_hash);
    
    RoutingTable& get_routing_table() { return routing_table_; }
    const RoutingTable& get_routing_table() const { return routing_table_; }
    
    // Request tracking
    std::optional<PendingRequest> get_pending_request(const Hash256& request_id) const;
    void cancel_request(const Hash256& request_id);
    size_t pending_request_count() const { return pending_requests_.size(); }
    
    // Callbacks
    using ContentReceivedCallback = std::function<void(const ContentHash&, const std::vector<uint8_t>&)>;
    using ContentNotFoundCallback = std::function<void(const ContentHash&)>;
    
    void set_content_received_callback(ContentReceivedCallback callback) {
        content_received_callback_ = callback;
    }
    
    void set_content_not_found_callback(ContentNotFoundCallback callback) {
        content_not_found_callback_ = callback;
    }
    
    // Statistics
    uint64_t requests_sent() const { return requests_sent_; }
    uint64_t requests_received() const { return requests_received_; }
    uint64_t responses_sent() const { return responses_sent_; }
    uint64_t responses_received() const { return responses_received_; }
    uint64_t forwards() const { return forwards_; }
    
    // Maintenance
    void cleanup_timed_out_requests();
    void update_statistics();

private:
    NodeID local_node_id_;
    RoutingTable routing_table_;
    
    // Pending requests
    std::map<Hash256, PendingRequest> pending_requests_;
    
    // Local content we can serve
    std::vector<ContentHash> local_content_;
    
    // Callbacks
    ContentReceivedCallback content_received_callback_;
    ContentNotFoundCallback content_not_found_callback_;
    
    // Statistics
    uint64_t requests_sent_;
    uint64_t requests_received_;
    uint64_t responses_sent_;
    uint64_t responses_received_;
    uint64_t forwards_;
    
    // Request ID generation
    uint64_t next_request_counter_;
    
    // Helpers
    Hash256 generate_request_id();
    NodeID select_next_hop(const ContentHash& content_hash) const;
    bool should_forward_request(const ContentRequest& request) const;
    bool can_serve_locally(const ContentHash& content_hash) const;
    
    // Onion routing helpers
    std::vector<std::vector<uint8_t>> create_onion_layers(
        const std::vector<NodeID>& route_path,
        const std::vector<uint8_t>& payload
    );
    
    std::optional<std::vector<uint8_t>> decrypt_onion_layer(
        const std::vector<uint8_t>& encrypted_layer
    );
    
    // TODO: Actual network sending (will integrate with SessionManager)
    void send_request_to_peer(const NodeID& peer_id, const ContentRequest& request);
    void send_response_to_peer(const NodeID& peer_id, const ContentResponse& response);
};

/**
 * RouterStatistics - Detailed routing statistics
 */
struct RouterStatistics {
    uint64_t total_requests_sent;
    uint64_t total_requests_received;
    uint64_t total_responses_sent;
    uint64_t total_responses_received;
    uint64_t total_forwards;
    uint64_t successful_retrievals;
    uint64_t failed_retrievals;
    uint64_t average_hop_count;
    size_t routing_table_size;
    size_t pending_requests_count;
    
    std::string to_string() const;
};

} // namespace cashew::network
