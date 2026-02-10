#include "activity_monitor.hpp"
#include <spdlog/spdlog.h>

namespace cashew::network {

// ActivityMonitor implementation
ActivityMonitor::ActivityMonitor(postake::ContributionTracker& tracker)
    : tracker_(tracker),
      monitoring_local_(false),
      total_connections_(0),
      total_bytes_routed_(0),
      total_things_hosted_(0),
      total_routes_monitored_(0)
{
    spdlog::info("ActivityMonitor initialized");
}

void ActivityMonitor::start_monitoring_local_node(const NodeID& local_node_id) {
    local_node_id_ = local_node_id;
    monitoring_local_ = true;
    
    // Report this node as online
    tracker_.record_node_online(local_node_id);
    
    spdlog::info("Started monitoring local node: {}", 
        local_node_id.to_string().substr(0, 8));
}

void ActivityMonitor::stop_monitoring_local_node() {
    if (local_node_id_.has_value()) {
        // Report this node as offline
        tracker_.record_node_offline(local_node_id_.value());
        
        spdlog::info("Stopped monitoring local node: {}",
            local_node_id_->to_string().substr(0, 8));
    }
    
    monitoring_local_ = false;
    local_node_id_.reset();
}

void ActivityMonitor::on_peer_connected(const NodeID& peer_id) {
    total_connections_++;
    
    // Report peer as online
    tracker_.record_node_online(peer_id);
    
    spdlog::debug("Peer connected: {}", peer_id.to_string().substr(0, 8));
}

void ActivityMonitor::on_peer_disconnected(const NodeID& peer_id) {
    // Report peer as offline
    tracker_.record_node_offline(peer_id);
    
    spdlog::debug("Peer disconnected: {}", peer_id.to_string().substr(0, 8));
}

void ActivityMonitor::on_session_established(const NodeID& peer_id) {
    // Session established means peer is definitely online and responsive
    tracker_.record_node_online(peer_id);
    
    spdlog::debug("Session established with: {}", peer_id.to_string().substr(0, 8));
}

void ActivityMonitor::on_session_closed(const NodeID& peer_id) {
    // Session closed - peer may or may not still be online
    // We'll let the connection layer handle the offline notification
    
    spdlog::debug("Session closed with: {}", peer_id.to_string().substr(0, 8));
}

void ActivityMonitor::on_bytes_sent(const NodeID& peer_id, uint64_t bytes) {
    // If we're monitoring local node, this is traffic FROM us TO peer
    if (monitoring_local_ && local_node_id_.has_value()) {
        tracker_.record_traffic(local_node_id_.value(), bytes, 0);
    }
    
    // The peer is receiving bytes
    tracker_.record_traffic(peer_id, 0, bytes);
}

void ActivityMonitor::on_bytes_received(const NodeID& peer_id, uint64_t bytes) {
    // If we're monitoring local node, this is traffic FROM peer TO us
    if (monitoring_local_ && local_node_id_.has_value()) {
        tracker_.record_traffic(local_node_id_.value(), 0, bytes);
    }
    
    // The peer is sending bytes
    tracker_.record_traffic(peer_id, bytes, 0);
}

void ActivityMonitor::on_bytes_routed_for(const NodeID& node_id, uint64_t bytes) {
    // This node routed traffic on behalf of others
    tracker_.record_bytes_routed(node_id, bytes);
    
    total_bytes_routed_ += bytes;
    
    spdlog::debug("Node {} routed {} bytes", 
        node_id.to_string().substr(0, 8), bytes);
}

void ActivityMonitor::on_thing_hosted(const NodeID& node_id, const Hash256& thing_hash, uint64_t size_bytes) {
    // Record hosting contribution
    tracker_.record_thing_hosted(node_id, size_bytes);
    
    // Track locally
    {
        std::lock_guard<std::mutex> lock(hosted_mutex_);
        hosted_things_[thing_hash] = size_bytes;
    }
    
    total_things_hosted_++;
    
    spdlog::info("Node {} now hosting Thing (hash) ({} bytes)",
        node_id.to_string().substr(0, 8),
        size_bytes);
}

void ActivityMonitor::on_thing_removed(const NodeID& node_id, const Hash256& thing_hash, uint64_t size_bytes) {
    // Record removal
    tracker_.record_thing_removed(node_id, size_bytes);
    
    // Remove from local tracking
    {
        std::lock_guard<std::mutex> lock(hosted_mutex_);
        hosted_things_.erase(thing_hash);
    }
    
    spdlog::info("Node {} removed Thing (hash) ({} bytes)",
        node_id.to_string().substr(0, 8),
        size_bytes);
}

void ActivityMonitor::on_route_successful(const NodeID& node_id, const Hash256& content_hash) {
    // Record successful routing
    tracker_.record_successful_route(node_id);
    
    total_routes_monitored_++;
    
    spdlog::debug("Node {} successfully routed content (hash)",
        node_id.to_string().substr(0, 8));
}

void ActivityMonitor::on_route_failed(const NodeID& node_id, const Hash256& content_hash) {
    // Record failed routing
    tracker_.record_failed_route(node_id);
    
    total_routes_monitored_++;
    
    spdlog::debug("Node {} failed to route content (hash)",
        node_id.to_string().substr(0, 8));
}

void ActivityMonitor::on_epoch_witnessed(const NodeID& node_id, uint64_t epoch) {
    // Record epoch participation
    tracker_.record_epoch_witness(node_id, epoch);
    
    spdlog::debug("Node {} witnessed epoch {}",
        node_id.to_string().substr(0, 8), epoch);
}

void ActivityMonitor::on_epoch_missed(const NodeID& node_id, uint64_t epoch) {
    // Record missed epoch
    tracker_.record_epoch_missed(node_id, epoch);
    
    spdlog::debug("Node {} missed epoch {}",
        node_id.to_string().substr(0, 8), epoch);
}

// MonitoredSession implementation
MonitoredSession::MonitoredSession(
    std::shared_ptr<Session> session,
    ActivityMonitor& monitor
)
    : session_(std::move(session)),
      monitor_(monitor),
      bytes_sent_(0),
      bytes_received_(0)
{
    // Notify monitor of session establishment
    monitor_.on_session_established(session_->get_remote_node_id());
}

MonitoredSession::~MonitoredSession() {
    // Notify monitor of session closure
    monitor_.on_session_closed(session_->get_remote_node_id());
}

std::optional<std::vector<uint8_t>> MonitoredSession::encrypt_and_send(
    const std::vector<uint8_t>& plaintext
) {
    auto ciphertext = session_->encrypt_message(plaintext);
    
    if (ciphertext.has_value()) {
        uint64_t bytes = ciphertext->size();
        bytes_sent_ += bytes;
        
        // Report to monitor
        monitor_.on_bytes_sent(session_->get_remote_node_id(), bytes);
    }
    
    return ciphertext;
}

std::optional<std::vector<uint8_t>> MonitoredSession::receive_and_decrypt(
    const std::vector<uint8_t>& ciphertext
) {
    uint64_t bytes = ciphertext.size();
    bytes_received_ += bytes;
    
    // Report to monitor
    monitor_.on_bytes_received(session_->get_remote_node_id(), bytes);
    
    return session_->decrypt_message(ciphertext);
}

// MonitoredConnectionManager implementation
MonitoredConnectionManager::MonitoredConnectionManager(
    std::shared_ptr<ConnectionManager> connection_mgr,
    ActivityMonitor& monitor
)
    : connection_mgr_(std::move(connection_mgr)),
      monitor_(monitor)
{
}

std::shared_ptr<Connection> MonitoredConnectionManager::create_monitored_connection(
    const SocketAddress& remote_addr,
    const NodeID& remote_node_id
) {
    // Create the connection
    auto conn = connection_mgr_->create_connection(remote_addr);
    
    if (conn) {
        // Set up callbacks to monitor activity
        
        // Data callback to track received bytes
        conn->set_data_callback([this, remote_node_id](
            const std::vector<uint8_t>& data
        ) {
            monitor_.on_bytes_received(remote_node_id, data.size());
        });
        
        // Connect callback
        conn->set_connected_callback([this, remote_node_id]() {
            monitor_.on_peer_connected(remote_node_id);
        });
        
        // Disconnect callback
        conn->set_disconnected_callback([this, remote_node_id]() {
            monitor_.on_peer_disconnected(remote_node_id);
        });
    }
    
    return conn;
}

} // namespace cashew::network
