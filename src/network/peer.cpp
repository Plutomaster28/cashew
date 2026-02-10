#include "network/peer.hpp"
#include "network/router.hpp"
#include "crypto/random.hpp"
#include "crypto/ed25519.hpp"
#include "crypto/blake3.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>

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

std::vector<NodeID> PeerDiscovery::select_random_peers(
    size_t count,
    const std::set<NodeID>& exclude
) const {
    // Collect all available peers
    std::vector<NodeID> available_peers;
    available_peers.reserve(discovered_peers_.size());
    
    for (const auto& [node_id, info] : discovered_peers_) {
        // Skip excluded peers
        if (exclude.find(node_id) != exclude.end()) {
            continue;
        }
        
        // Skip stale peers
        if (info.is_stale()) {
            continue;
        }
        
        available_peers.push_back(node_id);
    }
    
    // Shuffle using Fisher-Yates with crypto::Random
    for (size_t i = available_peers.size() - 1; i > 0; --i) {
        uint64_t j = crypto::Random::generate_uint64() % (i + 1);
        std::swap(available_peers[i], available_peers[j]);
    }
    
    // Return first N
    std::vector<NodeID> result;
    size_t num_results = std::min(count, available_peers.size());
    result.reserve(num_results);
    
    for (size_t i = 0; i < num_results; ++i) {
        result.push_back(available_peers[i]);
    }
    
    return result;
}

std::vector<NodeID> PeerDiscovery::select_diverse_peers(
    size_t count,
    const std::set<NodeID>& exclude,
    const PeerDiversity& current_diversity
) const {
    struct DiversityScore {
        NodeID node_id;
        float score;
        std::string subnet;
    };
    
    std::vector<DiversityScore> scored_peers;
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
        
        // Extract subnet
        std::string subnet = extract_subnet(info.address);
        
        // Calculate diversity score
        float score = 1.0f;
        
        // Penalize if subnet is already represented
        if (current_diversity.subnets.find(subnet) != current_diversity.subnets.end()) {
            score *= 0.3f;  // Strong penalty for same subnet
        }
        
        // Add reliability component
        score *= info.reliability_score();
        
        scored_peers.push_back({node_id, score, subnet});
    }
    
    // Sort by diversity score (descending)
    std::sort(scored_peers.begin(), scored_peers.end(),
        [](const DiversityScore& a, const DiversityScore& b) {
            return a.score > b.score;
        });
    
    // Select peers, tracking subnets
    std::vector<NodeID> result;
    std::set<std::string> selected_subnets = current_diversity.subnets;
    result.reserve(count);
    
    for (const auto& scored : scored_peers) {
        if (result.size() >= count) {
            break;
        }
        
        result.push_back(scored.node_id);
        selected_subnets.insert(scored.subnet);
    }
    
    return result;
}

PeerDiversity PeerDiscovery::calculate_diversity(const std::vector<NodeID>& connected_peers) const {
    PeerDiversity diversity;
    
    for (const auto& node_id : connected_peers) {
        auto peer_opt = get_peer_info(node_id);
        if (!peer_opt) {
            continue;
        }
        
        const auto& info = *peer_opt;
        
        // Extract subnet
        std::string subnet = extract_subnet(info.address);
        diversity.subnets.insert(subnet);
        
        // Track unique addresses
        diversity.unique_addresses.insert(info.address);
        
        // Estimate geographic distribution (simplified)
        // In production, would use GeoIP database
        diversity.estimated_geographic_regions++;
    }
    
    return diversity;
}

bool PeerDiscovery::save_to_disk(const std::string& filepath) const {
    try {
        nlohmann::json j;
        
        // Save bootstrap nodes
        nlohmann::json bootstrap_array = nlohmann::json::array();
        for (const auto& node : bootstrap_nodes_) {
            nlohmann::json node_obj;
            node_obj["address"] = node.address;
            node_obj["public_key"] = hash_to_hex(Hash256(node.public_key));
            node_obj["description"] = node.description;
            bootstrap_array.push_back(node_obj);
        }
        j["bootstrap_nodes"] = bootstrap_array;
        
        // Save discovered peers
        nlohmann::json peers_array = nlohmann::json::array();
        for (const auto& [node_id, info] : discovered_peers_) {
            nlohmann::json peer_obj;
            peer_obj["node_id"] = node_id.to_string();
            peer_obj["address"] = info.address;
            peer_obj["first_seen"] = info.first_seen;
            peer_obj["last_seen"] = info.last_seen;
            peer_obj["connection_attempts"] = info.connection_attempts;
            peer_obj["successful_connections"] = info.successful_connections;
            peer_obj["is_bootstrap"] = info.is_bootstrap;
            peers_array.push_back(peer_obj);
        }
        j["discovered_peers"] = peers_array;
        
        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            CASHEW_LOG_ERROR("Failed to open peer database file for writing: {}", filepath);
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        CASHEW_LOG_INFO("Saved peer database to {}", filepath);
        return true;
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Failed to save peer database: {}", e.what());
        return false;
    }
}

bool PeerDiscovery::load_from_disk(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            CASHEW_LOG_WARN("Peer database file not found: {}", filepath);
            return false;
        }
        
        nlohmann::json j;
        file >> j;
        file.close();
        
        // Load bootstrap nodes
        if (j.contains("bootstrap_nodes")) {
            for (const auto& node_obj : j["bootstrap_nodes"]) {
                BootstrapNode node;
                node.address = node_obj["address"];
                auto pubkey_hash = hex_to_hash(node_obj["public_key"]);
                std::copy(pubkey_hash.begin(), pubkey_hash.end(), node.public_key.begin());
                node.description = node_obj["description"];
                bootstrap_nodes_.push_back(node);
            }
        }
        
        // Load discovered peers
        if (j.contains("discovered_peers")) {
            for (const auto& peer_obj : j["discovered_peers"]) {
                PeerInfo info;
                info.node_id = NodeID::from_string(peer_obj["node_id"]);
                info.address = peer_obj["address"];
                info.first_seen = peer_obj["first_seen"];
                info.last_seen = peer_obj["last_seen"];
                info.connection_attempts = peer_obj["connection_attempts"];
                info.successful_connections = peer_obj["successful_connections"];
                info.is_bootstrap = peer_obj["is_bootstrap"];
                
                discovered_peers_[info.node_id] = info;
            }
        }
        
        CASHEW_LOG_INFO("Loaded peer database from {}", filepath);
        return true;
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Failed to load peer database: {}", e.what());
        return false;
    }
}

std::string PeerDiscovery::extract_subnet(const std::string& address) const {
    // Extract IP from address (format: "ip:port")
    size_t colon_pos = address.find(':');
    std::string ip = (colon_pos != std::string::npos) ? 
                     address.substr(0, colon_pos) : address;
    
    // Extract /24 subnet for IPv4
    size_t last_dot = ip.rfind('.');
    if (last_dot != std::string::npos) {
        return ip.substr(0, last_dot) + ".0/24";
    }
    
    // For IPv6, extract /48 prefix (simplified)
    size_t colons_found = 0;
    for (size_t i = 0; i < ip.length(); ++i) {
        if (ip[i] == ':') {
            colons_found++;
            if (colons_found == 3) {
                return ip.substr(0, i) + "::/48";
            }
        }
    }
    
    return ip;  // Fallback
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
    
    if (bootstrap_nodes.empty()) {
        CASHEW_LOG_WARN("No bootstrap nodes configured");
        return;
    }
    
    size_t connected = 0;
    size_t max_bootstrap = policy_.max_bootstrap_peers;
    
    for (const auto& node : bootstrap_nodes) {
        if (connected >= max_bootstrap) {
            break;
        }
        
        CASHEW_LOG_INFO("Connecting to bootstrap: {}", node.description);
        
        // Derive NodeID from public key (BLAKE3 hash)
        std::vector<uint8_t> pubkey_bytes(node.public_key.begin(), node.public_key.end());
        auto node_id_hash = crypto::Blake3::hash(pubkey_bytes);
        NodeID bootstrap_id(node_id_hash);
        
        // Add to discovery as bootstrap peer
        discovery_.add_discovered_peer(bootstrap_id, node.address);
        auto peer_info_opt = discovery_.get_peer_info(bootstrap_id);
        if (peer_info_opt) {
            auto& peer_info = const_cast<PeerInfo&>(*peer_info_opt);
            peer_info.is_bootstrap = true;
        }
        
        // Initiate connection
        connect_to_peer(bootstrap_id, node.address);
        
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

void PeerManager::handle_peer_announcement(const PeerAnnouncementMessage& msg) {
    // TODO: Get public key from message or session for verification
    // For now, skip signature verification
    
    // Check timestamp (reject if too old or in the future)
    uint64_t current_time = current_timestamp();
    uint64_t time_diff = (msg.timestamp > current_time) ? 
                        (msg.timestamp - current_time) : 
                        (current_time - msg.timestamp);
    
    if (time_diff > 300) {  // 5 minutes
        CASHEW_LOG_DEBUG("Received peer announcement with stale timestamp");
        return;
    }
    
    // Don't add ourselves
    if (msg.node_id == local_node_id_) {
        return;
    }
    
    // Add to discovered peers (use listen_address if provided, otherwise skip)
    if (!msg.listen_address.empty()) {
        discovery_.add_discovered_peer(msg.node_id, msg.listen_address);
        discovery_.update_peer_capabilities(msg.node_id, msg.capabilities);
        CASHEW_LOG_INFO("Received peer announcement from {}", msg.listen_address);
    }
}

void PeerManager::handle_peer_request(const NodeID& requesting_peer, const PeerRequestMessage& msg) {
    // Verify requester ID matches
    if (msg.requester_id != requesting_peer) {
        CASHEW_LOG_WARN("Peer request requester ID mismatch");
        return;
    }
    
    // Check timestamp
    uint64_t current_time = current_timestamp();
    uint64_t time_diff = (msg.timestamp > current_time) ? 
                        (msg.timestamp - current_time) : 
                        (current_time - msg.timestamp);
    
    if (time_diff > 300) {  // 5 minutes
        CASHEW_LOG_DEBUG("Received peer request with stale timestamp");
        return;
    }
    
    // Select peers to respond with
    std::set<NodeID> exclude;
    exclude.insert(requesting_peer);  // Don't send the requester back to themselves
    exclude.insert(local_node_id_);   // Don't send ourselves
    
    auto selected_peers = discovery_.select_random_peers(
        std::min(static_cast<size_t>(msg.max_peers), size_t(20)),  // Max 20 peers per response
        exclude
    );
    
    // Build response
    PeerResponseMessage response;
    response.responder_id = local_node_id_;
    response.timestamp = current_time;
    
    for (const auto& peer_id : selected_peers) {
        auto info_opt = discovery_.get_peer_info(peer_id);
        if (info_opt) {
            PeerResponseMessage::PeerEntry entry;
            entry.node_id = peer_id;
            entry.address = info_opt->address;
            entry.capabilities = info_opt->capabilities;
            entry.last_seen = info_opt->last_seen;
            response.peers.push_back(entry);
        }
    }
    
    // Send response (would integrate with session/router)
    auto response_bytes = response.to_bytes();
    send_to_peer(requesting_peer, response_bytes);
    
    CASHEW_LOG_DEBUG("Sent peer response with {} peers", response.peers.size());
}

void PeerManager::handle_peer_response(const PeerResponseMessage& msg) {
    // Verify responder ID (would need public key from responding peer)
    // For now, assume valid if we have an active connection
    
    if (!is_connected(msg.responder_id)) {
        CASHEW_LOG_DEBUG("Received peer response from non-connected peer");
        return;
    }
    
    // Check timestamp
    uint64_t current_time = current_timestamp();
    uint64_t time_diff = (msg.timestamp > current_time) ? 
                        (msg.timestamp - current_time) : 
                        (current_time - msg.timestamp);
    
    if (time_diff > 300) {  // 5 minutes
        CASHEW_LOG_DEBUG("Received peer response with stale timestamp");
        return;
    }
    
    // Add discovered peers
    size_t new_peers = 0;
    for (const auto& entry : msg.peers) {
        // Don't add ourselves
        if (entry.node_id == local_node_id_) {
            continue;
        }
        
        // Check if already known
        auto existing = discovery_.get_peer_info(entry.node_id);
        if (!existing) {
            new_peers++;
        }
        
        discovery_.add_discovered_peer(entry.node_id, entry.address);
        discovery_.update_peer_capabilities(entry.node_id, entry.capabilities);
    }
    
    CASHEW_LOG_INFO("Received peer response with {} peers ({} new)", 
                   msg.peers.size(), new_peers);
}

void PeerManager::handle_nat_traversal_request(const NodeID& requesting_peer, const NATTraversalRequest& req) {
    // Get the connection info to determine observed address
    auto conn_opt = get_connection(requesting_peer);
    if (!conn_opt) {
        CASHEW_LOG_DEBUG("NAT traversal request from non-connected peer");
        return;
    }
    
    // Verify requester ID matches
    if (req.requester_id != requesting_peer) {
        CASHEW_LOG_WARN("NAT traversal request requester ID mismatch");
        return;
    }
    
    // Extract observed address from connection
    // In a real implementation, would get this from the socket/session
    std::string observed_addr = conn_opt->address;
    
    // Extract IP and port
    size_t colon_pos = observed_addr.find(':');
    std::string ip = (colon_pos != std::string::npos) ? 
                     observed_addr.substr(0, colon_pos) : observed_addr;
    uint16_t port = (colon_pos != std::string::npos) ? 
                    static_cast<uint16_t>(std::stoi(observed_addr.substr(colon_pos + 1))) : 0;
    
    // Build response
    NATTraversalResponse response;
    response.public_address = ip;
    response.public_port = port;
    response.timestamp = current_timestamp();
    
    // Send response
    auto response_bytes = response.to_bytes();
    send_to_peer(requesting_peer, response_bytes);
    
    CASHEW_LOG_DEBUG("Sent NAT traversal response to peer");
}

bool PeerManager::save_peer_database(const std::string& filepath) const {
    return discovery_.save_to_disk(filepath);
}

bool PeerManager::load_peer_database(const std::string& filepath) {
    return const_cast<PeerDiscovery&>(discovery_).load_from_disk(filepath);
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

// PeerAnnouncementMessage methods

std::vector<uint8_t> PeerAnnouncementMessage::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Message type (1 byte)
    data.push_back(0x01);  // PEER_ANNOUNCEMENT
    
    // Node ID (32 bytes)
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    
    // Listen address length (2 bytes)
    uint16_t addr_len = static_cast<uint16_t>(listen_address.length());
    data.push_back((addr_len >> 8) & 0xFF);
    data.push_back(addr_len & 0xFF);
    
    // Listen address
    data.insert(data.end(), listen_address.begin(), listen_address.end());
    
    // Timestamp (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Capabilities - serialize as JSON
    nlohmann::json caps_json;
    caps_json["can_host_things"] = capabilities.can_host_things;
    caps_json["can_route_content"] = capabilities.can_route_content;
    caps_json["can_provide_storage"] = capabilities.can_provide_storage;
    std::string caps_str = caps_json.dump();
    
    // Capabilities length (2 bytes)
    uint16_t caps_len = static_cast<uint16_t>(caps_str.length());
    data.push_back((caps_len >> 8) & 0xFF);
    data.push_back(caps_len & 0xFF);
    
    // Capabilities data
    data.insert(data.end(), caps_str.begin(), caps_str.end());
    
    // Signature (64 bytes)
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<PeerAnnouncementMessage> PeerAnnouncementMessage::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 140) {  // Minimum size
        return std::nullopt;
    }
    
    size_t offset = 0;
    
    // Message type
    if (data[offset++] != 0x01) {
        return std::nullopt;
    }
    
    PeerAnnouncementMessage msg;
    
    // Node ID
    std::copy(data.begin() + offset, data.begin() + offset + 32, msg.node_id.id.begin());
    offset += 32;
    
    // Address length
    uint16_t addr_len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    if (offset + addr_len > data.size()) {
        return std::nullopt;
    }
    
    // Address
    msg.listen_address = std::string(data.begin() + offset, data.begin() + offset + addr_len);
    offset += addr_len;
    
    // Timestamp
    msg.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        msg.timestamp = (msg.timestamp << 8) | data[offset++];
    }
    
    // Capabilities length
    uint16_t caps_len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    if (offset + caps_len + 64 > data.size()) {
        return std::nullopt;
    }
    
    // Capabilities
    std::string caps_str(data.begin() + offset, data.begin() + offset + caps_len);
    offset += caps_len;
    
    try {
        nlohmann::json caps_json = nlohmann::json::parse(caps_str);
        msg.capabilities.can_host_things = caps_json["can_host_things"];
        msg.capabilities.can_route_content = caps_json["can_route_content"];
        msg.capabilities.can_provide_storage = caps_json["can_provide_storage"];
    } catch (...) {
        return std::nullopt;
    }
    
    // Signature
    std::copy(data.begin() + offset, data.begin() + offset + 64, msg.signature.begin());
    
    return msg;
}

bool PeerAnnouncementMessage::verify_signature(const PublicKey& public_key) const {
    // Create data to verify (everything except signature)
    std::vector<uint8_t> data;
    
    // Node ID
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    
    // Address
    data.insert(data.end(), listen_address.begin(), listen_address.end());
    
    // Timestamp
    for (int i = 7; i >= 0; --i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Verify signature
    return crypto::Ed25519::verify(data, signature, public_key);
}

// PeerRequestMessage methods

std::vector<uint8_t> PeerRequestMessage::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Message type (1 byte)
    data.push_back(0x02);  // PEER_REQUEST
    
    // Requester node ID (32 bytes)
    data.insert(data.end(), requester_id.id.begin(), requester_id.id.end());
    
    // Max peers (4 bytes)
    for (int i = 3; i >= 0; --i) {
        data.push_back((max_peers >> (i * 8)) & 0xFF);
    }
    
    // Timestamp (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Signature (64 bytes)
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<PeerRequestMessage> PeerRequestMessage::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() != 109) {  // Fixed size
        return std::nullopt;
    }
    
    size_t offset = 0;
    
    // Message type
    if (data[offset++] != 0x02) {
        return std::nullopt;
    }
    
    PeerRequestMessage msg;
    
    // Requester node ID
    std::copy(data.begin() + offset, data.begin() + offset + 32, msg.requester_id.id.begin());
    offset += 32;
    
    // Max peers
    msg.max_peers = 0;
    for (int i = 0; i < 4; ++i) {
        msg.max_peers = (msg.max_peers << 8) | data[offset++];
    }
    
    // Timestamp
    msg.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        msg.timestamp = (msg.timestamp << 8) | data[offset++];
    }
    
    // Signature
    std::copy(data.begin() + offset, data.begin() + offset + 64, msg.signature.begin());
    
    return msg;
}

// PeerResponseMessage methods

std::vector<uint8_t> PeerResponseMessage::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Message type (1 byte)
    data.push_back(0x03);  // PEER_RESPONSE
    
    // Responder node ID (32 bytes)
    data.insert(data.end(), responder_id.id.begin(), responder_id.id.end());
    
    // Peer count (2 bytes)
    uint16_t peer_count = static_cast<uint16_t>(peers.size());
    data.push_back((peer_count >> 8) & 0xFF);
    data.push_back(peer_count & 0xFF);
    
    // Peers
    for (const auto& peer : peers) {
        // Node ID (32 bytes)
        data.insert(data.end(), peer.node_id.id.begin(), peer.node_id.id.end());
        
        // Address length (2 bytes)
        uint16_t addr_len = static_cast<uint16_t>(peer.address.length());
        data.push_back((addr_len >> 8) & 0xFF);
        data.push_back(addr_len & 0xFF);
        
        // Address
        data.insert(data.end(), peer.address.begin(), peer.address.end());
        
        // Last seen (8 bytes)
        for (int i = 7; i >= 0; --i) {
            data.push_back((peer.last_seen >> (i * 8)) & 0xFF);
        }
    }
    
    // Timestamp (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Signature (64 bytes)
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<PeerResponseMessage> PeerResponseMessage::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 107) {  // Minimum size
        return std::nullopt;
    }
    
    size_t offset = 0;
    
    // Message type
    if (data[offset++] != 0x03) {
        return std::nullopt;
    }
    
    PeerResponseMessage msg;
    
    // Responder node ID
    std::copy(data.begin() + offset, data.begin() + offset + 32, msg.responder_id.id.begin());
    offset += 32;
    
    // Peer count
    uint16_t peer_count = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    // Peers
    for (uint16_t i = 0; i < peer_count; ++i) {
        if (offset + 42 > data.size()) {  // Minimum per peer
            return std::nullopt;
        }
        
        PeerResponseMessage::PeerEntry entry;
        
        // Node ID
        std::copy(data.begin() + offset, data.begin() + offset + 32, entry.node_id.id.begin());
        offset += 32;
        
        // Address length
        uint16_t addr_len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + addr_len > data.size()) {
            return std::nullopt;
        }
        
        // Address
        entry.address = std::string(data.begin() + offset, data.begin() + offset + addr_len);
        offset += addr_len;
        
        // Last seen
        entry.last_seen = 0;
        for (int j = 0; j < 8; ++j) {
            entry.last_seen = (entry.last_seen << 8) | data[offset++];
        }
        
        msg.peers.push_back(entry);
    }
    
    if (offset + 72 > data.size()) {
        return std::nullopt;
    }
    
    // Timestamp
    msg.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        msg.timestamp = (msg.timestamp << 8) | data[offset++];
    }
    
    // Signature
    std::copy(data.begin() + offset, data.begin() + offset + 64, msg.signature.begin());
    
    return msg;
}

// NAT Traversal methods

std::vector<uint8_t> NATTraversalRequest::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Message type (1 byte)
    data.push_back(0x04);  // NAT_TRAVERSAL_REQUEST
    
    // Requester node ID (32 bytes)
    data.insert(data.end(), requester_id.id.begin(), requester_id.id.end());
    
    // Nonce (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((nonce >> (i * 8)) & 0xFF);
    }
    
    // Timestamp (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    return data;
}

std::optional<NATTraversalRequest> NATTraversalRequest::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() != 49) {  // Fixed size
        return std::nullopt;
    }
    
    size_t offset = 0;
    
    // Message type
    if (data[offset++] != 0x04) {
        return std::nullopt;
    }
    
    NATTraversalRequest req;
    
    // Requester node ID
    std::copy(data.begin() + offset, data.begin() + offset + 32, req.requester_id.id.begin());
    offset += 32;
    
    // Nonce
    req.nonce = 0;
    for (int i = 0; i < 8; ++i) {
        req.nonce = (req.nonce << 8) | data[offset++];
    }
    
    // Timestamp
    req.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        req.timestamp = (req.timestamp << 8) | data[offset++];
    }
    
    return req;
}

std::vector<uint8_t> NATTraversalResponse::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Message type (1 byte)
    data.push_back(0x05);  // NAT_TRAVERSAL_RESPONSE
    
    // Public address length (2 bytes)
    uint16_t addr_len = static_cast<uint16_t>(public_address.length());
    data.push_back((addr_len >> 8) & 0xFF);
    data.push_back(addr_len & 0xFF);
    
    // Public address
    data.insert(data.end(), public_address.begin(), public_address.end());
    
    // Public port (2 bytes)
    data.push_back((public_port >> 8) & 0xFF);
    data.push_back(public_port & 0xFF);
    
    // Timestamp (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    return data;
}

std::optional<NATTraversalResponse> NATTraversalResponse::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 13) {  // Minimum size
        return std::nullopt;
    }
    
    size_t offset = 0;
    
    // Message type
    if (data[offset++] != 0x05) {
        return std::nullopt;
    }
    
    NATTraversalResponse resp;
    
    // Public address length
    uint16_t addr_len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    if (offset + addr_len + 10 > data.size()) {
        return std::nullopt;
    }
    
    // Public address
    resp.public_address = std::string(data.begin() + offset, data.begin() + offset + addr_len);
    offset += addr_len;
    
    // Public port
    resp.public_port = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    
    // Timestamp
    resp.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        resp.timestamp = (resp.timestamp << 8) | data[offset++];
    }
    
    return resp;
}

// PeerDiversity methods

bool PeerDiversity::is_diverse() const {
    // Check subnet diversity (should have at least 3 different subnets)
    if (subnets.size() < 3) {
        return false;
    }
    
    // Check address diversity (should have at least 5 unique addresses)
    if (unique_addresses.size() < 5) {
        return false;
    }
    
    // Check geographic diversity (simplified check)
    if (estimated_geographic_regions < 2) {
        return false;
    }
    
    return true;
}

} // namespace cashew::network
