#include "gossip.hpp"
#include "utils/logger.hpp"
#include "crypto/blake3.hpp"
#include "crypto/random.hpp"
#include <algorithm>
#include <cstring>

namespace cashew::network {

// NodeCapabilities implementation

std::vector<uint8_t> NodeCapabilities::to_bytes() const {
    std::vector<uint8_t> data;
    data.reserve(3 + 8 + 8);
    
    uint8_t flags = 0;
    if (can_host_things) flags |= 0x01;
    if (can_route_content) flags |= 0x02;
    if (can_provide_storage) flags |= 0x04;
    data.push_back(flags);
    
    // Storage capacity
    for (int i = 0; i < 8; ++i) {
        data.push_back((storage_capacity_bytes >> (i * 8)) & 0xFF);
    }
    
    // Bandwidth capacity
    for (int i = 0; i < 8; ++i) {
        data.push_back((bandwidth_capacity_mbps >> (i * 8)) & 0xFF);
    }
    
    return data;
}

std::optional<NodeCapabilities> NodeCapabilities::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 1 + 8 + 8) {
        return std::nullopt;
    }
    
    NodeCapabilities caps;
    size_t offset = 0;
    
    uint8_t flags = data[offset++];
    caps.can_host_things = (flags & 0x01) != 0;
    caps.can_route_content = (flags & 0x02) != 0;
    caps.can_provide_storage = (flags & 0x04) != 0;
    
    // Storage capacity
    caps.storage_capacity_bytes = 0;
    for (int i = 0; i < 8; ++i) {
        caps.storage_capacity_bytes |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Bandwidth capacity
    caps.bandwidth_capacity_mbps = 0;
    for (int i = 0; i < 8; ++i) {
        caps.bandwidth_capacity_mbps |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    return caps;
}

// PeerAnnouncement implementation

std::vector<uint8_t> PeerAnnouncement::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Node ID
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    
    // Public key
    data.insert(data.end(), public_key.begin(), public_key.end());
    
    // Capabilities
    auto caps_bytes = capabilities.to_bytes();
    data.insert(data.end(), caps_bytes.begin(), caps_bytes.end());
    
    // Timestamp
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Signature
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<PeerAnnouncement> PeerAnnouncement::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 32 + 32 + 17 + 8 + 64) {
        return std::nullopt;
    }
    
    PeerAnnouncement announcement;
    size_t offset = 0;
    
    // Node ID
    std::copy(data.begin() + offset, data.begin() + offset + 32,
             announcement.node_id.id.begin());
    offset += 32;
    
    // Public key
    std::copy(data.begin() + offset, data.begin() + offset + 32,
             announcement.public_key.begin());
    offset += 32;
    
    // Capabilities
    std::vector<uint8_t> caps_data(data.begin() + offset, data.begin() + offset + 17);
    auto caps = NodeCapabilities::from_bytes(caps_data);
    if (!caps) return std::nullopt;
    announcement.capabilities = *caps;
    offset += 17;
    
    // Timestamp
    announcement.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        announcement.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Signature
    std::copy(data.begin() + offset, data.begin() + offset + 64,
             announcement.signature.begin());
    
    return announcement;
}

// ContentAnnouncement implementation

std::vector<uint8_t> ContentAnnouncement::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Content hash
    data.insert(data.end(), content_hash.hash.begin(), content_hash.hash.end());
    
    // Content size
    for (int i = 0; i < 8; ++i) {
        data.push_back((content_size >> (i * 8)) & 0xFF);
    }
    
    // Hosting node
    data.insert(data.end(), hosting_node.id.begin(), hosting_node.id.end());
    
    // Network ID (optional)
    if (network_id.has_value()) {
        data.push_back(1);  // Has network ID
        data.insert(data.end(), network_id->begin(), network_id->end());
    } else {
        data.push_back(0);  // No network ID
    }
    
    // Timestamp
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Signature
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<ContentAnnouncement> ContentAnnouncement::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 32 + 8 + 32 + 1 + 8 + 64) {
        return std::nullopt;
    }
    
    ContentAnnouncement announcement;
    size_t offset = 0;
    
    // Content hash
    std::copy(data.begin() + offset, data.begin() + offset + 32,
             announcement.content_hash.hash.begin());
    offset += 32;
    
    // Content size
    announcement.content_size = 0;
    for (int i = 0; i < 8; ++i) {
        announcement.content_size |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Hosting node
    std::copy(data.begin() + offset, data.begin() + offset + 32,
             announcement.hosting_node.id.begin());
    offset += 32;
    
    // Network ID
    uint8_t has_network = data[offset++];
    if (has_network == 1) {
        if (data.size() < offset + 32) return std::nullopt;
        Hash256 net_id;
        std::copy(data.begin() + offset, data.begin() + offset + 32,
                 net_id.begin());
        announcement.network_id = net_id;
        offset += 32;
    }
    
    // Timestamp
    announcement.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        announcement.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Signature
    std::copy(data.begin() + offset, data.begin() + offset + 64,
             announcement.signature.begin());
    
    return announcement;
}

// NetworkStateUpdate implementation

std::vector<uint8_t> NetworkStateUpdate::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Epoch number
    for (int i = 0; i < 8; ++i) {
        data.push_back((epoch_number >> (i * 8)) & 0xFF);
    }
    
    // Active nodes count and list
    uint32_t node_count = static_cast<uint32_t>(active_nodes.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back((node_count >> (i * 8)) & 0xFF);
    }
    
    for (const auto& node : active_nodes) {
        data.insert(data.end(), node.id.begin(), node.id.end());
    }
    
    // PoW difficulty
    for (int i = 0; i < 4; ++i) {
        data.push_back((pow_difficulty >> (i * 8)) & 0xFF);
    }
    
    // Entropy seed
    uint32_t seed_size = static_cast<uint32_t>(entropy_seed.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back((seed_size >> (i * 8)) & 0xFF);
    }
    data.insert(data.end(), entropy_seed.begin(), entropy_seed.end());
    
    // Timestamp
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Signatures
    uint32_t sig_count = static_cast<uint32_t>(signatures.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back((sig_count >> (i * 8)) & 0xFF);
    }
    
    for (const auto& sig : signatures) {
        data.insert(data.end(), sig.begin(), sig.end());
    }
    
    return data;
}

std::optional<NetworkStateUpdate> NetworkStateUpdate::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 4 + 4 + 4 + 8 + 4) {
        return std::nullopt;
    }
    
    NetworkStateUpdate state;
    size_t offset = 0;
    
    // Epoch number
    state.epoch_number = 0;
    for (int i = 0; i < 8; ++i) {
        state.epoch_number |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Active nodes
    uint32_t node_count = 0;
    for (int i = 0; i < 4; ++i) {
        node_count |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    for (uint32_t i = 0; i < node_count; ++i) {
        if (offset + 32 > data.size()) return std::nullopt;
        NodeID node;
        std::copy(data.begin() + offset, data.begin() + offset + 32,
                 node.id.begin());
        state.active_nodes.push_back(node);
        offset += 32;
    }
    
    // PoW difficulty
    state.pow_difficulty = 0;
    for (int i = 0; i < 4; ++i) {
        state.pow_difficulty |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    // Entropy seed
    uint32_t seed_size = 0;
    for (int i = 0; i < 4; ++i) {
        seed_size |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    if (offset + seed_size > data.size()) return std::nullopt;
    state.entropy_seed.assign(data.begin() + offset, data.begin() + offset + seed_size);
    offset += seed_size;
    
    // Timestamp
    state.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        state.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Signatures
    uint32_t sig_count = 0;
    for (int i = 0; i < 4; ++i) {
        sig_count |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    for (uint32_t i = 0; i < sig_count; ++i) {
        if (offset + 64 > data.size()) return std::nullopt;
        Signature sig;
        std::copy(data.begin() + offset, data.begin() + offset + 64,
                 sig.begin());
        state.signatures.push_back(sig);
        offset += 64;
    }
    
    return state;
}

// GossipMessage implementation

std::vector<uint8_t> GossipMessage::to_bytes() const {
    std::vector<uint8_t> data;
    
    data.push_back(static_cast<uint8_t>(type));
    data.insert(data.end(), message_id.begin(), message_id.end());
    
    // Payload size
    uint32_t payload_size = static_cast<uint32_t>(payload.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back((payload_size >> (i * 8)) & 0xFF);
    }
    data.insert(data.end(), payload.begin(), payload.end());
    
    // Timestamp
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    data.push_back(hop_count);
    
    return data;
}

std::optional<GossipMessage> GossipMessage::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 1 + 32 + 4 + 8 + 1) {
        return std::nullopt;
    }
    
    GossipMessage msg;
    size_t offset = 0;
    
    msg.type = static_cast<GossipMessageType>(data[offset++]);
    
    std::copy(data.begin() + offset, data.begin() + offset + 32,
             msg.message_id.begin());
    offset += 32;
    
    uint32_t payload_size = 0;
    for (int i = 0; i < 4; ++i) {
        payload_size |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    if (offset + payload_size > data.size()) return std::nullopt;
    msg.payload.assign(data.begin() + offset, data.begin() + offset + payload_size);
    offset += payload_size;
    
    msg.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        msg.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    msg.hop_count = data[offset];
    
    return msg;
}

Hash256 GossipMessage::compute_id() const {
    auto msg_bytes = to_bytes();
    return crypto::Blake3::hash(msg_bytes);
}

bool GossipMessage::is_too_old() const {
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    return (now_seconds - timestamp) > MAX_AGE_SECONDS;
}

bool GossipMessage::has_exceeded_hops() const {
    return hop_count >= MAX_HOPS;
}

// GossipProtocol implementation

GossipProtocol::GossipProtocol(const NodeID& local_node_id)
    : local_node_id_(local_node_id),
      fanout_(DEFAULT_FANOUT),
      max_seen_messages_(DEFAULT_MAX_SEEN),
      messages_received_(0),
      messages_sent_(0) {
    CASHEW_LOG_INFO("Created gossip protocol for node {}",
                   crypto::Blake3::hash_to_hex(local_node_id.id));
}

void GossipProtocol::receive_message(const GossipMessage& message) {
    ++messages_received_;
    
    CASHEW_LOG_DEBUG("Received gossip message type {}",
                    static_cast<int>(message.type));
    
    if (!should_propagate(message)) {
        return;
    }
    
    // Mark as seen
    mark_as_seen(message.message_id);
    
    // Process locally
    invoke_handlers(message);
    
    // Propagate to random subset of peers
    auto forward_peers = get_random_peers(fanout_);
    
    GossipMessage forward_msg = message;
    forward_msg.hop_count++;
    
    for (const auto& peer : forward_peers) {
        send_to_peer(peer, forward_msg);
    }
}

void GossipProtocol::broadcast_message(const GossipMessage& message) {
    CASHEW_LOG_INFO("Broadcasting gossip message type {}",
                   static_cast<int>(message.type));
    
    mark_as_seen(message.message_id);
    
    auto forward_peers = get_random_peers(fanout_);
    
    for (const auto& peer : forward_peers) {
        send_to_peer(peer, message);
    }
    
    messages_sent_ += forward_peers.size();
}

GossipMessage GossipProtocol::create_peer_announcement(const NodeCapabilities& capabilities) {
    PeerAnnouncement announcement;
    announcement.node_id = local_node_id_;
    // TODO: Get actual public key
    announcement.public_key = PublicKey{};
    announcement.capabilities = capabilities;
    
    auto now = std::chrono::system_clock::now();
    announcement.timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    // TODO: Sign announcement
    announcement.signature = Signature{};
    
    GossipMessage message;
    message.type = GossipMessageType::PEER_ANNOUNCEMENT;
    message.payload = announcement.to_bytes();
    message.timestamp = announcement.timestamp;
    message.hop_count = 0;
    message.message_id = message.compute_id();
    
    return message;
}

GossipMessage GossipProtocol::create_content_announcement(
    const ContentHash& content_hash,
    uint64_t content_size,
    std::optional<Hash256> network_id) {
    
    ContentAnnouncement announcement;
    announcement.content_hash = content_hash;
    announcement.content_size = content_size;
    announcement.hosting_node = local_node_id_;
    announcement.network_id = network_id;
    
    auto now = std::chrono::system_clock::now();
    announcement.timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    // TODO: Sign announcement
    announcement.signature = Signature{};
    
    GossipMessage message;
    message.type = GossipMessageType::CONTENT_ANNOUNCEMENT;
    message.payload = announcement.to_bytes();
    message.timestamp = announcement.timestamp;
    message.hop_count = 0;
    message.message_id = message.compute_id();
    
    return message;
}

void GossipProtocol::register_handler(GossipMessageType type, GossipHandler handler) {
    handlers_.push_back({type, handler});
}

void GossipProtocol::unregister_handler(GossipMessageType type) {
    auto it = std::remove_if(handlers_.begin(), handlers_.end(),
        [type](const auto& pair) { return pair.first == type; });
    handlers_.erase(it, handlers_.end());
}

void GossipProtocol::add_peer(const NodeID& peer_id) {
    // Check if already added
    for (const auto& peer : peers_) {
        if (peer == peer_id) {
            return;
        }
    }
    
    peers_.push_back(peer_id);
    CASHEW_LOG_DEBUG("Added gossip peer (total: {})", peers_.size());
}

void GossipProtocol::remove_peer(const NodeID& peer_id) {
    auto it = std::remove(peers_.begin(), peers_.end(), peer_id);
    peers_.erase(it, peers_.end());
}

std::vector<NodeID> GossipProtocol::get_random_peers(size_t count) const {
    if (peers_.empty()) {
        return {};
    }
    
    size_t actual_count = std::min(count, peers_.size());
    std::vector<NodeID> selected;
    
    // Simple random selection (could be improved with Fisher-Yates)
    std::vector<NodeID> available = peers_;
    
    for (size_t i = 0; i < actual_count; ++i) {
        if (available.empty()) break;
        
        uint32_t index = crypto::Random::uniform(static_cast<uint32_t>(available.size()));
        selected.push_back(available[index]);
        available.erase(available.begin() + index);
    }
    
    return selected;
}

void GossipProtocol::cleanup_old_seen_messages() {
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    auto it = std::remove_if(seen_messages_.begin(), seen_messages_.end(),
        [now_seconds](const SeenMessage& msg) {
            return (now_seconds - msg.timestamp) > SEEN_MESSAGE_TTL_SECONDS;
        });
    
    size_t removed = std::distance(it, seen_messages_.end());
    seen_messages_.erase(it, seen_messages_.end());
    
    if (removed > 0) {
        CASHEW_LOG_DEBUG("Cleaned up {} old seen messages", removed);
    }
}

bool GossipProtocol::has_seen_message(const Hash256& message_id) const {
    for (const auto& seen : seen_messages_) {
        if (seen.message_id == message_id) {
            return true;
        }
    }
    return false;
}

bool GossipProtocol::should_propagate(const GossipMessage& message) {
    // Check if already seen
    if (has_seen_message(message.message_id)) {
        CASHEW_LOG_DEBUG("Message already seen, not propagating");
        return false;
    }
    
    // Check age
    if (message.is_too_old()) {
        CASHEW_LOG_DEBUG("Message too old, not propagating");
        return false;
    }
    
    // Check hop limit
    if (message.has_exceeded_hops()) {
        CASHEW_LOG_DEBUG("Message exceeded hop limit, not propagating");
        return false;
    }
    
    return true;
}

void GossipProtocol::mark_as_seen(const Hash256& message_id) {
    // Check if cache is full
    if (seen_messages_.size() >= max_seen_messages_) {
        // Remove oldest
        seen_messages_.erase(seen_messages_.begin());
    }
    
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    SeenMessage seen;
    seen.message_id = message_id;
    seen.timestamp = now_seconds;
    
    seen_messages_.push_back(seen);
}

void GossipProtocol::invoke_handlers(const GossipMessage& message) {
    for (const auto& [type, handler] : handlers_) {
        if (type == message.type) {
            handler(message);
        }
    }
}

void GossipProtocol::send_to_peer(const NodeID& peer_id, const GossipMessage& message) {
    (void)message;
    // TODO: Actual network sending through SessionManager
    CASHEW_LOG_DEBUG("Would send gossip to peer {}",
                    crypto::Blake3::hash_to_hex(peer_id.id));
    ++messages_sent_;
}

// GossipScheduler implementation

GossipScheduler::GossipScheduler(GossipProtocol& protocol)
    : protocol_(protocol),
      running_(false),
      peer_announcement_interval_(DEFAULT_PEER_INTERVAL_SECONDS),
      state_update_interval_(DEFAULT_STATE_INTERVAL_SECONDS),
      last_peer_announcement_(0),
      last_state_update_(0) {
}

void GossipScheduler::start() {
    running_ = true;
    CASHEW_LOG_INFO("Started gossip scheduler");
}

void GossipScheduler::stop() {
    running_ = false;
    CASHEW_LOG_INFO("Stopped gossip scheduler");
}

void GossipScheduler::announce_peer(const NodeCapabilities& capabilities) {
    auto message = protocol_.create_peer_announcement(capabilities);
    protocol_.broadcast_message(message);
    
    auto now = std::chrono::system_clock::now();
    last_peer_announcement_ = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

void GossipScheduler::announce_content(const ContentHash& content_hash, uint64_t content_size) {
    auto message = protocol_.create_content_announcement(content_hash, content_size);
    protocol_.broadcast_message(message);
}

} // namespace cashew::network
