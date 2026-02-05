#include "security/ip_protection.hpp"
#include "crypto/random.hpp"
#include "crypto/blake3.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <random>
#include <sstream>

namespace cashew::security {

// ConnectionInfo methods

uint64_t ConnectionInfo::connection_duration() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current = static_cast<uint64_t>(now_time_t);
    
    return current - connection_time;
}

bool ConnectionInfo::is_stale(uint64_t stale_threshold_seconds) const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current = static_cast<uint64_t>(now_time_t);
    
    return (current - last_activity_time) > stale_threshold_seconds;
}

// EphemeralAddress methods

bool EphemeralAddress::is_expired() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current = static_cast<uint64_t>(now_time_t);
    
    return current >= expiry_time;
}

uint64_t EphemeralAddress::time_remaining() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current = static_cast<uint64_t>(now_time_t);
    
    if (current >= expiry_time) {
        return 0;
    }
    return expiry_time - current;
}

// PeerRotationManager methods

PeerRotationManager::PeerRotationManager(const PeerRotationPolicy& policy)
    : policy_(policy),
      last_rotation_time_(current_timestamp()),
      total_rotations_(0),
      forced_rotations_(0)
{
    CASHEW_LOG_INFO("PeerRotationManager initialized (max duration: {}s, rotation: {}%)",
                   policy_.max_connection_duration_seconds,
                   static_cast<int>(policy_.rotation_percentage * 100));
}

void PeerRotationManager::register_connection(
    const NodeID& peer_id,
    const std::string& ip_address,
    uint16_t port
) {
    ConnectionInfo info;
    info.peer_id = peer_id;
    info.ip_address = ip_address;
    info.port = port;
    info.connection_time = current_timestamp();
    info.last_activity_time = info.connection_time;
    
    connections_[peer_id] = info;
    
    CASHEW_LOG_DEBUG("Registered connection to {}:{}", ip_address, port);
}

void PeerRotationManager::update_activity(
    const NodeID& peer_id,
    size_t bytes_sent,
    size_t bytes_received
) {
    auto it = connections_.find(peer_id);
    if (it != connections_.end()) {
        it->second.last_activity_time = current_timestamp();
        it->second.bytes_sent += bytes_sent;
        it->second.bytes_received += bytes_received;
        
        if (bytes_sent > 0) {
            it->second.messages_sent++;
        }
        if (bytes_received > 0) {
            it->second.messages_received++;
        }
    }
}

void PeerRotationManager::unregister_connection(const NodeID& peer_id) {
    auto it = connections_.find(peer_id);
    if (it != connections_.end()) {
        CASHEW_LOG_DEBUG("Unregistered connection (duration: {}s)",
                        it->second.connection_duration());
        connections_.erase(it);
    }
}

std::vector<NodeID> PeerRotationManager::select_peers_for_rotation() {
    std::vector<NodeID> to_rotate;
    
    // Don't rotate if we don't have enough peers
    if (connections_.size() < policy_.min_peer_pool_size) {
        return to_rotate;
    }
    
    // Check if it's time for rotation
    uint64_t current = current_timestamp();
    if (current - last_rotation_time_ < policy_.rotation_interval_seconds) {
        return to_rotate;
    }
    
    // Collect peers that should be rotated
    std::vector<std::pair<NodeID, uint64_t>> candidates;
    
    for (const auto& [peer_id, conn] : connections_) {
        if (meets_rotation_criteria(conn)) {
            candidates.push_back({peer_id, conn.connection_duration()});
        }
    }
    
    // Sort by connection duration (longest first)
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Select percentage of peers to rotate
    size_t num_to_rotate = static_cast<size_t>(
        connections_.size() * policy_.rotation_percentage
    );
    num_to_rotate = std::min(num_to_rotate, candidates.size());
    
    for (size_t i = 0; i < num_to_rotate; ++i) {
        to_rotate.push_back(candidates[i].first);
    }
    
    if (!to_rotate.empty()) {
        last_rotation_time_ = current;
        total_rotations_ += to_rotate.size();
        
        CASHEW_LOG_INFO("Selected {} peers for rotation", to_rotate.size());
    }
    
    return to_rotate;
}

bool PeerRotationManager::should_rotate_peer(const NodeID& peer_id) const {
    auto it = connections_.find(peer_id);
    if (it == connections_.end()) {
        return false;
    }
    
    return meets_rotation_criteria(it->second);
}

uint64_t PeerRotationManager::time_until_next_rotation() const {
    uint64_t current = current_timestamp();
    uint64_t elapsed = current - last_rotation_time_;
    
    if (elapsed >= policy_.rotation_interval_seconds) {
        return 0;
    }
    
    return policy_.rotation_interval_seconds - elapsed;
}

std::vector<ConnectionInfo> PeerRotationManager::get_all_connections() const {
    std::vector<ConnectionInfo> result;
    result.reserve(connections_.size());
    
    for (const auto& [_, conn] : connections_) {
        result.push_back(conn);
    }
    
    return result;
}

std::optional<ConnectionInfo> PeerRotationManager::get_connection_info(const NodeID& peer_id) const {
    auto it = connections_.find(peer_id);
    if (it != connections_.end()) {
        return it->second;
    }
    return std::nullopt;
}

uint64_t PeerRotationManager::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

bool PeerRotationManager::meets_rotation_criteria(const ConnectionInfo& conn) const {
    uint64_t duration = conn.connection_duration();
    
    // Too new, don't rotate yet
    if (duration < policy_.min_connection_duration_seconds) {
        return false;
    }
    
    // Too old, must rotate
    if (duration >= policy_.max_connection_duration_seconds) {
        return true;
    }
    
    // Inactive too long
    if (conn.is_stale(policy_.activity_timeout_seconds)) {
        return true;
    }
    
    return false;
}

// EphemeralAddressManager methods

EphemeralAddressManager::EphemeralAddressManager(uint64_t default_ttl_seconds)
    : default_ttl_seconds_(default_ttl_seconds),
      rotation_count_(0)
{
    current_own_ephemeral_id_ = generate_ephemeral_id();
    CASHEW_LOG_INFO("EphemeralAddressManager initialized (TTL: {}s)", default_ttl_seconds_);
}

EphemeralAddress EphemeralAddressManager::create_address(
    const NodeID& node_id,
    uint64_t ttl_seconds
) {
    if (ttl_seconds == 0) {
        ttl_seconds = default_ttl_seconds_;
    }
    
    EphemeralAddress addr;
    addr.ephemeral_id = generate_ephemeral_id();
    addr.actual_node_id = node_id;
    addr.creation_time = current_timestamp();
    addr.expiry_time = addr.creation_time + ttl_seconds;
    
    addresses_[addr.ephemeral_id] = addr;
    
    CASHEW_LOG_DEBUG("Created ephemeral address (TTL: {}s)", ttl_seconds);
    
    return addr;
}

std::optional<NodeID> EphemeralAddressManager::resolve_address(const Hash256& ephemeral_id) {
    auto it = addresses_.find(ephemeral_id);
    if (it == addresses_.end()) {
        return std::nullopt;
    }
    
    if (it->second.is_expired()) {
        addresses_.erase(it);
        return std::nullopt;
    }
    
    return it->second.actual_node_id;
}

void EphemeralAddressManager::revoke_address(const Hash256& ephemeral_id) {
    addresses_.erase(ephemeral_id);
    CASHEW_LOG_DEBUG("Revoked ephemeral address");
}

void EphemeralAddressManager::cleanup_expired() {
    std::vector<Hash256> to_remove;
    
    for (const auto& [id, addr] : addresses_) {
        if (addr.is_expired()) {
            to_remove.push_back(id);
        }
    }
    
    for (const auto& id : to_remove) {
        addresses_.erase(id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_DEBUG("Cleaned up {} expired ephemeral addresses", to_remove.size());
    }
}

Hash256 EphemeralAddressManager::get_current_ephemeral_id() const {
    return current_own_ephemeral_id_;
}

void EphemeralAddressManager::rotate_own_address() {
    current_own_ephemeral_id_ = generate_ephemeral_id();
    rotation_count_++;
    
    CASHEW_LOG_INFO("Rotated own ephemeral address (rotation #{})", rotation_count_);
}

uint64_t EphemeralAddressManager::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

Hash256 EphemeralAddressManager::generate_ephemeral_id() const {
    // Generate random 32 bytes
    auto random_bytes = crypto::Random::generate(32);
    
    // Hash for uniformity
    return crypto::Blake3::hash(random_bytes);
}

// TrafficPaddingEngine methods

TrafficPaddingEngine::TrafficPaddingEngine()
    : padding_enabled_(true),
      dummy_traffic_enabled_(false),
      jitter_enabled_(true)
{
}

std::vector<uint8_t> TrafficPaddingEngine::add_padding(const std::vector<uint8_t>& message) {
    if (!padding_enabled_) {
        return message;
    }
    
    size_t padding_size = calculate_padding_size(message.size());
    
    std::vector<uint8_t> padded = message;
    
    // Add padding length (2 bytes)
    uint16_t pad_len = static_cast<uint16_t>(padding_size);
    padded.push_back(static_cast<uint8_t>(pad_len & 0xFF));
    padded.push_back(static_cast<uint8_t>((pad_len >> 8) & 0xFF));
    
    // Add random padding
    auto padding_bytes = crypto::Random::generate(padding_size);
    padded.insert(padded.end(), padding_bytes.begin(), padding_bytes.end());
    
    return padded;
}

std::optional<std::vector<uint8_t>> TrafficPaddingEngine::remove_padding(
    const std::vector<uint8_t>& padded_message
) {
    if (!padding_enabled_ || padded_message.size() < 2) {
        return padded_message;
    }
    
    // Extract padding length from last 2 bytes before padding
    size_t message_end = padded_message.size() - 2;
    
    if (message_end < 2) {
        return std::nullopt;
    }
    
    // Read padding length
    uint16_t pad_len = static_cast<uint16_t>(padded_message[message_end - 2]) |
                       (static_cast<uint16_t>(padded_message[message_end - 1]) << 8);
    
    if (message_end < 2 + static_cast<size_t>(pad_len)) {
        return std::nullopt;
    }
    
    // Extract original message
    size_t original_size = message_end - 2;
    std::vector<uint8_t> original(padded_message.begin(), padded_message.begin() + original_size);
    
    return original;
}

uint64_t TrafficPaddingEngine::calculate_jitter_delay() const {
    if (!jitter_enabled_) {
        return 0;
    }
    
    // Random delay 0-100ms
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(0, 100);
    
    return dis(gen);
}

bool TrafficPaddingEngine::should_send_dummy() const {
    if (!dummy_traffic_enabled_) {
        return false;
    }
    
    // 5% chance of sending dummy
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, 100);
    
    return dis(gen) <= 5;
}

std::vector<uint8_t> TrafficPaddingEngine::generate_dummy_message() {
    // Random size 100-1000 bytes
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(100, 1000);
    
    size_t size = dis(gen);
    return crypto::Random::generate(size);
}

size_t TrafficPaddingEngine::calculate_padding_size(size_t message_size) const {
    // Round up to nearest 128 bytes to hide exact message size
    size_t block_size = 128;
    size_t padded_size = ((message_size + block_size - 1) / block_size) * block_size;
    
    // Additional random padding 0-64 bytes
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, 64);
    
    size_t random_extra = dis(gen);
    
    return (padded_size - message_size) + random_extra;
}

// IPProtectionCoordinator methods

IPProtectionCoordinator::IPProtectionCoordinator(
    const NodeID& local_node_id,
    const PeerRotationPolicy& rotation_policy
)
    : local_node_id_(local_node_id),
      rotation_manager_(rotation_policy),
      ephemeral_manager_(),
      padding_engine_(),
      peer_rotation_enabled_(true),
      ephemeral_addressing_enabled_(true),
      last_tick_time_(current_timestamp()),
      messages_padded_(0),
      dummy_messages_sent_(0)
{
    CASHEW_LOG_INFO("IPProtectionCoordinator initialized");
}

void IPProtectionCoordinator::on_connection_established(
    const NodeID& peer_id,
    const std::string& ip,
    uint16_t port
) {
    if (peer_rotation_enabled_) {
        rotation_manager_.register_connection(peer_id, ip, port);
    }
}

void IPProtectionCoordinator::on_connection_closed(const NodeID& peer_id) {
    if (peer_rotation_enabled_) {
        rotation_manager_.unregister_connection(peer_id);
    }
}

void IPProtectionCoordinator::on_data_sent(const NodeID& peer_id, size_t bytes) {
    if (peer_rotation_enabled_) {
        rotation_manager_.update_activity(peer_id, bytes, 0);
    }
}

void IPProtectionCoordinator::on_data_received(const NodeID& peer_id, size_t bytes) {
    if (peer_rotation_enabled_) {
        rotation_manager_.update_activity(peer_id, 0, bytes);
    }
}

void IPProtectionCoordinator::tick() {
    uint64_t current = current_timestamp();
    
    // Cleanup expired ephemeral addresses every 60 seconds
    if (ephemeral_addressing_enabled_ && (current - last_tick_time_) >= 60) {
        ephemeral_manager_.cleanup_expired();
    }
    
    // Rotate own ephemeral address every 30 minutes
    if (ephemeral_addressing_enabled_ && (current - last_tick_time_) >= 1800) {
        ephemeral_manager_.rotate_own_address();
    }
    
    last_tick_time_ = current;
}

std::vector<NodeID> IPProtectionCoordinator::get_peers_to_rotate() {
    if (!peer_rotation_enabled_) {
        return {};
    }
    
    return rotation_manager_.select_peers_for_rotation();
}

EphemeralAddress IPProtectionCoordinator::create_ephemeral_address(const NodeID& node_id) {
    return ephemeral_manager_.create_address(node_id);
}

std::optional<NodeID> IPProtectionCoordinator::resolve_ephemeral_address(const Hash256& ephemeral_id) {
    return ephemeral_manager_.resolve_address(ephemeral_id);
}

std::vector<uint8_t> IPProtectionCoordinator::prepare_outgoing_message(
    const std::vector<uint8_t>& message
) {
    auto padded = padding_engine_.add_padding(message);
    
    if (padded.size() != message.size()) {
        messages_padded_++;
    }
    
    return padded;
}

std::optional<std::vector<uint8_t>> IPProtectionCoordinator::process_incoming_message(
    const std::vector<uint8_t>& message
) {
    return padding_engine_.remove_padding(message);
}

void IPProtectionCoordinator::enable_traffic_padding(bool enable) {
    padding_engine_.set_padding_enabled(enable);
}

IPProtectionCoordinator::Statistics IPProtectionCoordinator::get_statistics() const {
    Statistics stats;
    stats.total_rotations = rotation_manager_.get_total_rotations();
    stats.active_connections = rotation_manager_.active_connection_count();
    stats.active_ephemeral_addresses = ephemeral_manager_.active_address_count();
    stats.messages_padded = messages_padded_;
    stats.dummy_messages_sent = dummy_messages_sent_;
    
    return stats;
}

std::string IPProtectionCoordinator::Statistics::to_string() const {
    std::ostringstream oss;
    oss << "IP Protection Statistics:\n";
    oss << "  Total rotations: " << total_rotations << "\n";
    oss << "  Active connections: " << active_connections << "\n";
    oss << "  Active ephemeral addresses: " << active_ephemeral_addresses << "\n";
    oss << "  Messages padded: " << messages_padded << "\n";
    oss << "  Dummy messages sent: " << dummy_messages_sent;
    return oss.str();
}

uint64_t IPProtectionCoordinator::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

} // namespace cashew::security
