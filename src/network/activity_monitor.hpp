#pragma once

#include "cashew/common.hpp"
#include "core/postake/postake.hpp"
#include "network/session.hpp"
#include "network/connection.hpp"
#include <memory>
#include <atomic>

namespace cashew::network {

/**
 * ActivityMonitor - Monitors network activity and reports to ContributionTracker
 * 
 * This class serves as the integration layer between the network layer
 * and the PoStake contribution tracking system. It monitors:
 * - Node online/offline events
 * - Bytes routed through this node
 * - Traffic sent/received
 * - Thing hosting activity
 * - Routing success/failure
 */
class ActivityMonitor {
public:
    explicit ActivityMonitor(postake::ContributionTracker& tracker);
    ~ActivityMonitor() = default;
    
    // Node lifecycle monitoring
    void on_peer_connected(const NodeID& peer_id);
    void on_peer_disconnected(const NodeID& peer_id);
    void on_session_established(const NodeID& peer_id);
    void on_session_closed(const NodeID& peer_id);
    
    // Traffic monitoring
    void on_bytes_sent(const NodeID& peer_id, uint64_t bytes);
    void on_bytes_received(const NodeID& peer_id, uint64_t bytes);
    void on_bytes_routed_for(const NodeID& node_id, uint64_t bytes);
    
    // Content monitoring
    void on_thing_hosted(const NodeID& node_id, const Hash256& thing_hash, uint64_t size_bytes);
    void on_thing_removed(const NodeID& node_id, const Hash256& thing_hash, uint64_t size_bytes);
    
    // Routing monitoring
    void on_route_successful(const NodeID& node_id, const Hash256& content_hash);
    void on_route_failed(const NodeID& node_id, const Hash256& content_hash);
    
    // Epoch monitoring
    void on_epoch_witnessed(const NodeID& node_id, uint64_t epoch);
    void on_epoch_missed(const NodeID& node_id, uint64_t epoch);
    
    // Local node monitoring (this node's own activity)
    void start_monitoring_local_node(const NodeID& local_node_id);
    void stop_monitoring_local_node();
    
    // Statistics
    uint64_t total_connections_monitored() const { return total_connections_; }
    uint64_t total_bytes_routed() const { return total_bytes_routed_; }
    uint64_t total_things_hosted() const { return total_things_hosted_; }
    uint64_t total_routes_monitored() const { return total_routes_monitored_; }
    
private:
    postake::ContributionTracker& tracker_;
    
    // Local node tracking
    std::optional<NodeID> local_node_id_;
    std::atomic<bool> monitoring_local_;
    
    // Statistics
    std::atomic<uint64_t> total_connections_;
    std::atomic<uint64_t> total_bytes_routed_;
    std::atomic<uint64_t> total_things_hosted_;
    std::atomic<uint64_t> total_routes_monitored_;
    
    // Track hosted Things
    std::map<Hash256, uint64_t> hosted_things_;  // thing_hash -> size_bytes
    std::mutex hosted_mutex_;
};

/**
 * MonitoredSession - Session wrapper that reports activity to ActivityMonitor
 */
class MonitoredSession {
public:
    MonitoredSession(
        std::shared_ptr<Session> session,
        ActivityMonitor& monitor
    );
    ~MonitoredSession();
    
    // Delegate to underlying session
    Session* operator->() { return session_.get(); }
    const Session* operator->() const { return session_.get(); }
    
    Session& get_session() { return *session_; }
    const Session& get_session() const { return *session_; }
    
    // Monitored operations
    std::optional<std::vector<uint8_t>> encrypt_and_send(const std::vector<uint8_t>& plaintext);
    std::optional<std::vector<uint8_t>> receive_and_decrypt(const std::vector<uint8_t>& ciphertext);
    
private:
    std::shared_ptr<Session> session_;
    ActivityMonitor& monitor_;
    
    std::atomic<uint64_t> bytes_sent_;
    std::atomic<uint64_t> bytes_received_;
};

/**
 * MonitoredConnectionManager - ConnectionManager that reports activity
 */
class MonitoredConnectionManager {
public:
    MonitoredConnectionManager(
        std::shared_ptr<ConnectionManager> connection_mgr,
        ActivityMonitor& monitor
    );
    ~MonitoredConnectionManager() = default;
    
    // Delegate to underlying manager
    ConnectionManager* operator->() { return connection_mgr_.get(); }
    const ConnectionManager* operator->() const { return connection_mgr_.get(); }
    
    // Monitored connection creation
    std::shared_ptr<Connection> create_monitored_connection(
        const SocketAddress& remote_addr,
        const NodeID& remote_node_id
    );
    
private:
    std::shared_ptr<ConnectionManager> connection_mgr_;
    ActivityMonitor& monitor_;
};

} // namespace cashew::network
