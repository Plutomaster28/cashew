#pragma once

#include "cashew/common.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <chrono>

namespace cashew::security {

/**
 * ConnectionInfo - Track connection metadata for analysis
 */
struct ConnectionInfo {
    NodeID peer_id;
    std::string ip_address;
    uint16_t port;
    uint64_t connection_time;
    uint64_t last_activity_time;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t messages_sent;
    uint32_t messages_received;
    
    ConnectionInfo()
        : port(0), connection_time(0), last_activity_time(0),
          bytes_sent(0), bytes_received(0),
          messages_sent(0), messages_received(0) {}
    
    uint64_t connection_duration() const;
    bool is_stale(uint64_t stale_threshold_seconds) const;
};

/**
 * PeerRotationPolicy - Rules for when to rotate peers
 */
struct PeerRotationPolicy {
    uint64_t max_connection_duration_seconds;  // Rotate after this duration
    uint64_t min_connection_duration_seconds;  // Don't rotate too quickly
    uint64_t activity_timeout_seconds;         // Rotate if inactive
    uint64_t rotation_interval_seconds;        // Periodic rotation check
    size_t min_peer_pool_size;                 // Minimum peers before rotating
    float rotation_percentage;                 // % of peers to rotate per cycle
    
    // Default policy: rotate connections every 30 min, keep active for at least 5 min
    PeerRotationPolicy()
        : max_connection_duration_seconds(1800),  // 30 minutes
          min_connection_duration_seconds(300),    // 5 minutes
          activity_timeout_seconds(600),           // 10 minutes
          rotation_interval_seconds(120),          // Check every 2 minutes
          min_peer_pool_size(8),                   // Need at least 8 peers
          rotation_percentage(0.25f) {}            // Rotate 25% at a time
};

/**
 * EphemeralAddress - Temporary addressing for peer connections
 * 
 * Design:
 * - Each connection gets temporary identifier
 * - Identifiers rotate periodically
 * - No correlation between sessions
 * - Prevents network mapping
 */
struct EphemeralAddress {
    Hash256 ephemeral_id;        // Temporary connection identifier
    uint64_t creation_time;
    uint64_t expiry_time;
    NodeID actual_node_id;       // Real node ID (kept private)
    
    bool is_expired() const;
    uint64_t time_remaining() const;
};

/**
 * IPObfuscationStrategy - Methods for hiding IP patterns
 */
enum class IPObfuscationStrategy {
    RANDOM_ROTATION,      // Random peer selection for rotation
    GEOGRAPHIC_DIVERSITY, // Prefer geographically diverse peers
    AS_DIVERSITY,         // Prefer different autonomous systems
    TIMING_JITTER,        // Add random delays to prevent timing analysis
    TRAFFIC_PADDING       // Add dummy traffic to mask real patterns
};

/**
 * PeerRotationManager - Manages dynamic peer rotation
 * 
 * Responsibilities:
 * - Track connection durations
 * - Identify stale connections
 * - Select peers for rotation
 * - Maintain peer diversity
 * - Prevent traffic analysis
 * 
 * Security goals:
 * - No long-lived connections (prevents correlation)
 * - Geographic diversity (prevents localization)
 * - Traffic mixing (prevents pattern analysis)
 * - Connection cycling (prevents fingerprinting)
 */
class PeerRotationManager {
public:
    explicit PeerRotationManager(const PeerRotationPolicy& policy = PeerRotationPolicy());
    ~PeerRotationManager() = default;
    
    // Connection tracking
    void register_connection(const NodeID& peer_id, const std::string& ip_address, uint16_t port);
    void update_activity(const NodeID& peer_id, size_t bytes_sent, size_t bytes_received);
    void unregister_connection(const NodeID& peer_id);
    
    // Rotation logic
    std::vector<NodeID> select_peers_for_rotation();
    bool should_rotate_peer(const NodeID& peer_id) const;
    uint64_t time_until_next_rotation() const;
    
    // Queries
    std::vector<ConnectionInfo> get_all_connections() const;
    std::optional<ConnectionInfo> get_connection_info(const NodeID& peer_id) const;
    size_t active_connection_count() const { return connections_.size(); }
    
    // Statistics
    uint64_t get_total_rotations() const { return total_rotations_; }
    uint64_t get_forced_rotations() const { return forced_rotations_; }
    
    // Policy management
    void update_policy(const PeerRotationPolicy& policy) { policy_ = policy; }
    const PeerRotationPolicy& get_policy() const { return policy_; }
    
private:
    PeerRotationPolicy policy_;
    std::map<NodeID, ConnectionInfo> connections_;
    uint64_t last_rotation_time_;
    uint64_t total_rotations_;
    uint64_t forced_rotations_;
    
    uint64_t current_timestamp() const;
    bool meets_rotation_criteria(const ConnectionInfo& conn) const;
};

/**
 * EphemeralAddressManager - Manages temporary connection identifiers
 * 
 * Design:
 * - Generate new ephemeral ID for each session
 * - Rotate IDs periodically
 * - Never reuse IDs
 * - Map ephemeral -> actual node ID internally
 * 
 * Security properties:
 * - No linkability between sessions
 * - Prevents network mapping
 * - Thwarts surveillance
 * - Preserves anonymity
 */
class EphemeralAddressManager {
public:
    explicit EphemeralAddressManager(uint64_t default_ttl_seconds = 3600);
    ~EphemeralAddressManager() = default;
    
    /**
     * Create ephemeral address for a node
     * @param node_id Actual node ID
     * @param ttl_seconds Time-to-live for this address
     * @return Ephemeral address
     */
    EphemeralAddress create_address(const NodeID& node_id, uint64_t ttl_seconds = 0);
    
    /**
     * Resolve ephemeral ID to actual node ID
     * @param ephemeral_id Temporary identifier
     * @return Actual node ID if valid and not expired
     */
    std::optional<NodeID> resolve_address(const Hash256& ephemeral_id);
    
    /**
     * Revoke an ephemeral address
     */
    void revoke_address(const Hash256& ephemeral_id);
    
    /**
     * Cleanup expired addresses
     */
    void cleanup_expired();
    
    /**
     * Get our current ephemeral ID for outgoing connections
     */
    Hash256 get_current_ephemeral_id() const;
    
    /**
     * Rotate our ephemeral ID
     */
    void rotate_own_address();
    
    // Statistics
    size_t active_address_count() const { return addresses_.size(); }
    uint64_t get_rotation_count() const { return rotation_count_; }
    
private:
    std::map<Hash256, EphemeralAddress> addresses_;
    Hash256 current_own_ephemeral_id_;
    NodeID own_node_id_;
    uint64_t default_ttl_seconds_;
    uint64_t rotation_count_;
    
    uint64_t current_timestamp() const;
    Hash256 generate_ephemeral_id() const;
};

/**
 * TrafficPaddingEngine - Add dummy traffic to prevent analysis
 * 
 * Techniques:
 * - Random message sizes
 * - Random timing between messages
 * - Dummy messages to mask real traffic
 * - Constant-rate shaping (optional)
 */
class TrafficPaddingEngine {
public:
    TrafficPaddingEngine();
    ~TrafficPaddingEngine() = default;
    
    /**
     * Add padding to message
     * @param message Original message
     * @return Padded message (with random bytes added)
     */
    std::vector<uint8_t> add_padding(const std::vector<uint8_t>& message);
    
    /**
     * Remove padding from message
     * @param padded_message Message with padding
     * @return Original message
     */
    std::optional<std::vector<uint8_t>> remove_padding(const std::vector<uint8_t>& padded_message);
    
    /**
     * Calculate timing jitter
     * @return Milliseconds to delay before sending
     */
    uint64_t calculate_jitter_delay() const;
    
    /**
     * Should send dummy message?
     * @return true if we should send dummy traffic
     */
    bool should_send_dummy() const;
    
    /**
     * Generate dummy message
     * @return Random dummy data
     */
    std::vector<uint8_t> generate_dummy_message();
    
    // Configuration
    void set_padding_enabled(bool enabled) { padding_enabled_ = enabled; }
    void set_dummy_traffic_enabled(bool enabled) { dummy_traffic_enabled_ = enabled; }
    void set_jitter_enabled(bool enabled) { jitter_enabled_ = enabled; }
    
private:
    bool padding_enabled_;
    bool dummy_traffic_enabled_;
    bool jitter_enabled_;
    
    size_t calculate_padding_size(size_t message_size) const;
};

/**
 * IPProtectionCoordinator - High-level IP protection orchestration
 * 
 * Integrates:
 * - Peer rotation
 * - Ephemeral addressing
 * - Traffic padding
 * - Timing obfuscation
 * 
 * Provides unified interface for all IP protection mechanisms
 */
class IPProtectionCoordinator {
public:
    IPProtectionCoordinator(
        const NodeID& local_node_id,
        const PeerRotationPolicy& rotation_policy = PeerRotationPolicy()
    );
    ~IPProtectionCoordinator() = default;
    
    // Connection management
    void on_connection_established(const NodeID& peer_id, const std::string& ip, uint16_t port);
    void on_connection_closed(const NodeID& peer_id);
    void on_data_sent(const NodeID& peer_id, size_t bytes);
    void on_data_received(const NodeID& peer_id, size_t bytes);
    
    // Periodic maintenance
    void tick();  // Call regularly (e.g., every second)
    
    // Protection operations
    std::vector<NodeID> get_peers_to_rotate();
    EphemeralAddress create_ephemeral_address(const NodeID& node_id);
    std::optional<NodeID> resolve_ephemeral_address(const Hash256& ephemeral_id);
    
    // Message processing
    std::vector<uint8_t> prepare_outgoing_message(const std::vector<uint8_t>& message);
    std::optional<std::vector<uint8_t>> process_incoming_message(const std::vector<uint8_t>& message);
    
    // Configuration
    void enable_peer_rotation(bool enable) { peer_rotation_enabled_ = enable; }
    void enable_ephemeral_addressing(bool enable) { ephemeral_addressing_enabled_ = enable; }
    void enable_traffic_padding(bool enable);
    
    // Statistics
    struct Statistics {
        uint64_t total_rotations;
        uint64_t active_connections;
        uint64_t active_ephemeral_addresses;
        uint64_t messages_padded;
        uint64_t dummy_messages_sent;
        
        std::string to_string() const;
    };
    
    Statistics get_statistics() const;
    
private:
    NodeID local_node_id_;
    PeerRotationManager rotation_manager_;
    EphemeralAddressManager ephemeral_manager_;
    TrafficPaddingEngine padding_engine_;
    
    bool peer_rotation_enabled_;
    bool ephemeral_addressing_enabled_;
    
    uint64_t last_tick_time_;
    uint64_t messages_padded_;
    uint64_t dummy_messages_sent_;
    
    uint64_t current_timestamp() const;
};

} // namespace cashew::security
