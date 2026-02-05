#include "network/peer.hpp"
#include "network/router.hpp"
#include "crypto/random.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <sstream>

namespace cashew::network {

// PeerInfo methods

float PeerInfo::reliability_score() const {
    if (connection_attempts == 0) {
        return 1.0f;  // New peer, optimistic
    }
    
    float success_rate = static_cast<float>(successful_connections) / 
                        static_cast<float>(connection_attempts);
    
    // Penalize bootstrap nodes less for failures
    if (is_bootstrap) {
        success_rate = 0.5f * success_rate + 0.5f;
    }
    
    return success_rate;
}

bool PeerInfo::is_stale() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_time = static_cast<uint64_t>(now_time_t);
    
    static constexpr uint64_t PEER_STALE_TIMEOUT = 3600;  // 1 hour
    uint64_t age = current_time - last_seen;
    return age > PEER_STALE_TIMEOUT;
}

// PeerConnection methods

bool PeerConnection::is_idle() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_time = static_cast<uint64_t>(now_time_t);
    
    uint64_t idle_seconds = current_time - last_activity;
    return idle_seconds > 300;  // 5 minutes
}

std::chrono::seconds PeerConnection::idle_duration() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_time = static_cast<uint64_t>(now_time_t);
    
    uint64_t idle_seconds = current_time - last_activity;
    return std::chrono::seconds(idle_seconds);
}

// PeerDiscovery methods

void PeerDiscovery::add_bootstrap_node(const BootstrapNode& node) {
    bootstrap_nodes_.push_back(node);
    CASHEW_LOG_INFO("Added bootstrap node: {}", node.description);
}

std::vector<BootstrapNode> PeerDiscovery::get_bootstrap_nodes() const {
    return bootstrap_nodes_;
}

void PeerDiscovery::add_discovered_peer(const NodeID& node_id, const std::string& address) {
    auto it = discovered_peers_.find(node_id);
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_time = static_cast<uint64_t>(now_time_t);
    
    if (it == discovered_peers_.end()) {
        // New peer
        PeerInfo info;
        info.node_id = node_id;
        info.address = address;
        info.first_seen = current_time;
        info.last_seen = current_time;
        
        discovered_peers_[node_id] = info;
        CASHEW_LOG_DEBUG("Discovered new peer at {}", address);
    } else {
        // Update existing
        it->second.address = address;
        it->second.last_seen = current_time;
    }
}

void PeerDiscovery::update_peer_seen(const NodeID& node_id) {
    auto it = discovered_peers_.find(node_id);
    if (it == discovered_peers_.end()) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    it->second.last_seen = static_cast<uint64_t>(now_time_t);
}

void PeerDiscovery::update_peer_capabilities(const NodeID& node_id, const NodeCapabilities& caps) {
    auto it = discovered_peers_.find(node_id);
    if (it == discovered_peers_.end()) {
        return;
    }
    
    it->second.capabilities = caps;
    CASHEW_LOG_DEBUG("Updated peer capabilities");
}

std::vector<PeerInfo> PeerDiscovery::get_discovered_peers() const {
    std::vector<PeerInfo> result;
    result.reserve(discovered_peers_.size());
    
    for (const auto& [node_id, info] : discovered_peers_) {
        result.push_back(info);
    }
    
    return result;
}

std::optional<PeerInfo> PeerDiscovery::get_peer_info(const NodeID& node_id) const {
    auto it = discovered_peers_.find(node_id);
    if (it == discovered_peers_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<NodeID> PeerDiscovery::select_peers_to_connect(
    size_t count,
    const std::set<NodeID>& exclude
) const {
    // Score all available peers
    struct ScoredPeer {
        NodeID node_id;
        float score;
    };
    
    std::vector<ScoredPeer> scored_peers;
    scored_peers.reserve(discovered_peers_.size());
    
    for (const auto& [node_id, info] : discovered_peers_) {
        // Skip excluded peers
        if (exclude.find(node_id) != exclude.end()) {
            continue;
        }
        
        // Skip stale peers
        if (info.is_stale()) {
            continue;
        }
        
        // Calculate score
        float reliability = info.reliability_score();
        float recency = 1.0f;  // Could factor in last_seen
        
        // Bootstrap nodes get priority
        float bootstrap_bonus = info.is_bootstrap ? 0.5f : 0.0f;
        
        float score = reliability * recency + bootstrap_bonus;
        scored_peers.push_back({node_id, score});
    }
    
    // Sort by score (descending)
    std::sort(scored_peers.begin(), scored_peers.end(),
        [](const ScoredPeer& a, const ScoredPeer& b) {
            return a.score > b.score;
        });
    
    // Return top N
    std::vector<NodeID> result;
    size_t num_results = std::min(count, scored_peers.size());
    result.reserve(num_results);
    
    for (size_t i = 0; i < num_results; ++i) {
        result.push_back(scored_peers[i].node_id);
    }
    
    return result;
}

void PeerDiscovery::cleanup_stale_peers() {
    std::vector<NodeID> to_remove;
    
    for (const auto& [node_id, info] : discovered_peers_) {
        if (info.is_stale() && !info.is_bootstrap) {
            to_remove.push_back(node_id);
        }
    }
    
    for (const auto& node_id : to_remove) {
        discovered_peers_.erase(node_id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_DEBUG("Cleaned up {} stale peers", to_remove.size());
    }
}

// PeerManager methods

PeerManager::PeerManager(
    const NodeID& local_node_id,
    SessionManager& session_manager,
    GossipProtocol& gossip_protocol
)
    : local_node_id_(local_node_id),
      session_manager_(session_manager),
      gossip_protocol_(gossip_protocol),
      total_connections_made_(0),
      total_connection_failures_(0)
{
    CASHEW_LOG_INFO("PeerManager initialized");
}

PeerManager::~PeerManager() {
    disconnect_all();
}

void PeerManager::add_bootstrap_node(const BootstrapNode& node) {
    discovery_.add_bootstrap_node(node);
}

void PeerManager::connect_to_bootstrap_nodes() {
    auto bootstrap_nodes = discovery_.get_bootstrap_nodes();
    
    size_t connected = 0;
    size_t max_bootstrap = policy_.max_bootstrap_peers;
    
    for (const auto& node : bootstrap_nodes) {
        if (connected >= max_bootstrap) {
            break;
        }
        
        // TODO: Derive NodeID from public key
        // For now, we'll need to connect and get NodeID from handshake
        CASHEW_LOG_INFO("Connecting to bootstrap: {}", node.description);
        
        // Add to discovery
        NodeID bootstrap_id{};  // Placeholder - will be filled during handshake
        discovery_.add_discovered_peer(bootstrap_id, node.address);
        
        // TODO: Implement actual connection
        // connect_to_peer(bootstrap_id, node.address);
        
        connected++;
    }
    
    CASHEW_LOG_INFO("Initiated connections to {} bootstrap nodes", connected);
}

void PeerManager::add_discovered_peer(const NodeID& node_id, const std::string& address) {
    discovery_.add_discovered_peer(node_id, address);
}

void PeerManager::update_peer_capabilities(const NodeID& node_id, const NodeCapabilities& caps) {
    discovery_.update_peer_capabilities(node_id, caps);
}

void PeerManager::connect_to_peer(const NodeID& peer_id, const std::string& address) {
    // Check if already connected
    if (is_connected(peer_id)) {
        CASHEW_LOG_DEBUG("Already connected to peer");
        return;
    }
    
    // Check if we can connect more peers
    if (!can_connect_more_peers()) {
        CASHEW_LOG_DEBUG("Max peer limit reached");
        return;
    }
    
    // Check if we should connect to this peer
    if (!should_connect_to_peer(peer_id)) {
        CASHEW_LOG_DEBUG("Policy check failed for peer connection");
        return;
    }
    
    // Update last attempt time
    last_connection_attempt_[peer_id] = current_timestamp();
    
    // TODO: Implement actual connection via SessionManager
    // For now, this is a stub
    CASHEW_LOG_INFO("Connecting to peer at {}", address);
    
    // Simulate connection (in real implementation, this would be async)
    (void)address;
    // Session* session = session_manager_.initiate_session(peer_id, address);
    // if (session) {
    //     handle_successful_connection(peer_id, session);
    // } else {
    //     handle_failed_connection(peer_id);
    // }
}

void PeerManager::disconnect_peer(const NodeID& peer_id) {
    auto it = active_connections_.find(peer_id);
    if (it == active_connections_.end()) {
        return;
    }
    
    CASHEW_LOG_DEBUG("Disconnecting peer");
    
    // Notify callback
    if (peer_disconnected_callback_) {
        peer_disconnected_callback_(peer_id);
    }
    
    cleanup_connection(peer_id);
}

void PeerManager::disconnect_all() {
    std::vector<NodeID> peers;
    peers.reserve(active_connections_.size());
    
    for (const auto& [peer_id, conn] : active_connections_) {
        peers.push_back(peer_id);
    }
    
    for (const auto& peer_id : peers) {
        disconnect_peer(peer_id);
    }
    
    CASHEW_LOG_INFO("Disconnected all peers");
}

bool PeerManager::is_connected(const NodeID& peer_id) const {
    return active_connections_.find(peer_id) != active_connections_.end();
}

std::optional<PeerConnection> PeerManager::get_connection(const NodeID& peer_id) const {
    auto it = active_connections_.find(peer_id);
    if (it == active_connections_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<PeerConnection> PeerManager::get_all_connections() const {
    std::vector<PeerConnection> result;
    result.reserve(active_connections_.size());
    
    for (const auto& [peer_id, conn] : active_connections_) {
        result.push_back(conn);
    }
    
    return result;
}

void PeerManager::maintain_peer_connections() {
    // Cleanup stale discovered peers
    discovery_.cleanup_stale_peers();
    
    // Cleanup idle connections
    cleanup_idle_connections();
    
    // Try to maintain target peer count
    size_t current_count = active_connection_count();
    
    if (current_count < policy_.target_peers) {
        size_t needed = policy_.target_peers - current_count;
        
        // Get already connected peers
        std::set<NodeID> exclude;
        for (const auto& [peer_id, conn] : active_connections_) {
            exclude.insert(peer_id);
        }
        exclude.insert(local_node_id_);  // Don't connect to ourselves
        
        // Select peers to connect
        auto peers_to_connect = discovery_.select_peers_to_connect(needed, exclude);
        
        for (const auto& peer_id : peers_to_connect) {
            auto peer_info_opt = discovery_.get_peer_info(peer_id);
            if (peer_info_opt) {
                connect_to_peer(peer_id, peer_info_opt->address);
            }
        }
        
        if (!peers_to_connect.empty()) {
            CASHEW_LOG_DEBUG("Attempting to connect to {} new peers", peers_to_connect.size());
        }
    }
}

void PeerManager::cleanup_idle_connections() {
    std::vector<NodeID> to_disconnect;
    
    for (const auto& [peer_id, conn] : active_connections_) {
        if (conn.is_idle()) {
            auto idle_duration = conn.idle_duration();
            if (idle_duration.count() > static_cast<int64_t>(policy_.idle_timeout_seconds)) {
                to_disconnect.push_back(peer_id);
            }
        }
    }
    
    for (const auto& peer_id : to_disconnect) {
        CASHEW_LOG_DEBUG("Disconnecting idle peer");
        disconnect_peer(peer_id);
    }
}

void PeerManager::attempt_reconnections() {
    // Get peers that failed connection
    std::vector<NodeID> to_retry;
    
    uint64_t current_time = current_timestamp();
    
    for (const auto& [peer_id, last_attempt] : last_connection_attempt_) {
        // Skip if already connected
        if (is_connected(peer_id)) {
            continue;
        }
        
        // Check if enough time has passed
        uint64_t time_since_attempt = current_time - last_attempt;
        if (time_since_attempt < policy_.reconnect_delay_seconds) {
            continue;
        }
        
        // Check failure count
        auto failure_it = connection_failure_count_.find(peer_id);
        if (failure_it != connection_failure_count_.end()) {
            if (failure_it->second >= policy_.max_connection_attempts) {
                continue;  // Too many failures
            }
        }
        
        to_retry.push_back(peer_id);
    }
    
    // Limit retry attempts
    size_t max_retries = 5;
    if (to_retry.size() > max_retries) {
        to_retry.resize(max_retries);
    }
    
    for (const auto& peer_id : to_retry) {
        auto peer_info_opt = discovery_.get_peer_info(peer_id);
        if (peer_info_opt) {
            CASHEW_LOG_DEBUG("Retrying connection to peer");
            connect_to_peer(peer_id, peer_info_opt->address);
        }
    }
}

bool PeerManager::send_to_peer(const NodeID& peer_id, const std::vector<uint8_t>& data) {
    auto it = active_connections_.find(peer_id);
    if (it == active_connections_.end()) {
        return false;
    }
    
    auto& conn = it->second;
    
    // TODO: Integrate with Session to actually send
    // For now, this is a stub
    (void)data;
    
    // Update stats
    conn.bytes_sent += data.size();
    conn.last_activity = current_timestamp();
    
    CASHEW_LOG_DEBUG("Sent {} bytes to peer", data.size());
    return true;
}

void PeerManager::broadcast_to_peers(const std::vector<uint8_t>& data) {
    size_t sent_count = 0;
    
    for (const auto& [peer_id, conn] : active_connections_) {
        if (send_to_peer(peer_id, data)) {
            sent_count++;
        }
    }
    
    CASHEW_LOG_DEBUG("Broadcast to {} peers", sent_count);
}

void PeerManager::send_to_random_peers(const std::vector<uint8_t>& data, size_t count) {
    std::vector<NodeID> peer_ids;
    peer_ids.reserve(active_connections_.size());
    
    for (const auto& [peer_id, conn] : active_connections_) {
        peer_ids.push_back(peer_id);
    }
    
    // Shuffle using Fisher-Yates with crypto::Random
    for (size_t i = peer_ids.size() - 1; i > 0; --i) {
        uint64_t j = crypto::Random::generate_uint64() % (i + 1);
        std::swap(peer_ids[i], peer_ids[j]);
    }
    
    // Send to first N
    size_t num_to_send = std::min(count, peer_ids.size());
    size_t sent_count = 0;
    
    for (size_t i = 0; i < num_to_send; ++i) {
        if (send_to_peer(peer_ids[i], data)) {
            sent_count++;
        }
    }
    
    CASHEW_LOG_DEBUG("Sent to {} random peers", sent_count);
}

uint64_t PeerManager::total_bytes_sent() const {
    uint64_t total = 0;
    for (const auto& [peer_id, conn] : active_connections_) {
        total += conn.bytes_sent;
    }
    return total;
}

uint64_t PeerManager::total_bytes_received() const {
    uint64_t total = 0;
    for (const auto& [peer_id, conn] : active_connections_) {
        total += conn.bytes_received;
    }
    return total;
}

bool PeerManager::should_connect_to_peer(const NodeID& peer_id) const {
    // Don't connect to ourselves
    if (peer_id == local_node_id_) {
        return false;
    }
    
    // Check failure count
    auto failure_it = connection_failure_count_.find(peer_id);
    if (failure_it != connection_failure_count_.end()) {
        if (failure_it->second >= policy_.max_connection_attempts) {
            return false;
        }
    }
    
    // Check peer reliability
    auto peer_info_opt = discovery_.get_peer_info(peer_id);
    if (peer_info_opt) {
        float reliability = peer_info_opt->reliability_score();
        if (reliability < policy_.min_reliability_score) {
            return false;
        }
    }
    
    return true;
}

bool PeerManager::can_connect_more_peers() const {
    return active_connection_count() < policy_.max_peers;
}

void PeerManager::handle_successful_connection(const NodeID& peer_id, Session* session) {
    PeerConnection conn;
    conn.peer_id = peer_id;
    conn.session = session;
    conn.connected_at = current_timestamp();
    conn.last_activity = current_timestamp();
    conn.is_outbound = true;
    
    auto peer_info_opt = discovery_.get_peer_info(peer_id);
    if (peer_info_opt) {
        conn.address = peer_info_opt->address;
        
        // Update peer info
        auto& peer_info = const_cast<PeerInfo&>(*peer_info_opt);
        peer_info.connection_attempts++;
        peer_info.successful_connections++;
    }
    
    active_connections_[peer_id] = conn;
    total_connections_made_++;
    
    // Reset failure count
    connection_failure_count_[peer_id] = 0;
    
    CASHEW_LOG_INFO("Successfully connected to peer (total: {})", active_connection_count());
    
    // Notify callback
    if (peer_connected_callback_) {
        peer_connected_callback_(peer_id);
    }
}

void PeerManager::handle_failed_connection(const NodeID& peer_id) {
    auto peer_info_opt = discovery_.get_peer_info(peer_id);
    if (peer_info_opt) {
        auto& peer_info = const_cast<PeerInfo&>(*peer_info_opt);
        peer_info.connection_attempts++;
    }
    
    // Increment failure count
    connection_failure_count_[peer_id]++;
    total_connection_failures_++;
    
    CASHEW_LOG_WARN("Failed to connect to peer (failures: {})", 
                    connection_failure_count_[peer_id]);
}

void PeerManager::cleanup_connection(const NodeID& peer_id) {
    active_connections_.erase(peer_id);
}

uint64_t PeerManager::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

// PeerStatistics methods

std::string PeerStatistics::to_string() const {
    std::ostringstream oss;
    oss << "Peer Statistics:\n";
    oss << "  Active connections: " << active_connections << "\n";
    oss << "  Discovered peers: " << discovered_peers << "\n";
    oss << "  Bootstrap nodes: " << bootstrap_nodes << "\n";
    oss << "  Total connections made: " << total_connections_made << "\n";
    oss << "  Total connection failures: " << total_connection_failures << "\n";
    oss << "  Connection success rate: " << (average_connection_success_rate * 100.0f) << "%\n";
    oss << "  Total bytes sent: " << total_bytes_sent << "\n";
    oss << "  Total bytes received: " << total_bytes_received << "\n";
    oss << "  Average connection duration: " << average_connection_duration.count() << "s";
    return oss.str();
}

} // namespace cashew::network
