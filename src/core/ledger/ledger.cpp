#include "core/ledger/ledger.hpp"
#include "crypto/blake3.hpp"
#include "crypto/ed25519.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>

namespace cashew::ledger {

// LedgerEvent methods

std::vector<uint8_t> LedgerEvent::to_bytes() const {
    std::vector<uint8_t> data_out;
    
    // Event ID (32 bytes)
    data_out.insert(data_out.end(), event_id.begin(), event_id.end());
    
    // Event type (1 byte)
    data_out.push_back(static_cast<uint8_t>(event_type));
    
    // Source node (32 bytes)
    data_out.insert(data_out.end(), source_node.id.begin(), source_node.id.end());
    
    // Timestamp (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data_out.push_back(static_cast<uint8_t>(timestamp >> (i * 8)));
    }
    
    // Epoch (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data_out.push_back(static_cast<uint8_t>(epoch >> (i * 8)));
    }
    
    // Previous hash (32 bytes)
    data_out.insert(data_out.end(), previous_hash.begin(), previous_hash.end());
    
    // Data length (4 bytes)
    uint32_t data_len = static_cast<uint32_t>(data.size());
    for (int i = 0; i < 4; ++i) {
        data_out.push_back(static_cast<uint8_t>(data_len >> (i * 8)));
    }
    
    // Data
    data_out.insert(data_out.end(), data.begin(), data.end());
    
    // Signature (64 bytes)
    data_out.insert(data_out.end(), signature.begin(), signature.end());
    
    return data_out;
}

std::optional<LedgerEvent> LedgerEvent::from_bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 32 + 1 + 32 + 8 + 8 + 32 + 4 + 64) {
        return std::nullopt;
    }
    
    LedgerEvent event;
    size_t offset = 0;
    
    // Event ID
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, event.event_id.begin());
    offset += 32;
    
    // Event type
    event.event_type = static_cast<EventType>(bytes[offset++]);
    
    // Source node
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, event.source_node.id.begin());
    offset += 32;
    
    // Timestamp
    event.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        event.timestamp |= static_cast<uint64_t>(bytes[offset++]) << (i * 8);
    }
    
    // Epoch
    event.epoch = 0;
    for (int i = 0; i < 8; ++i) {
        event.epoch |= static_cast<uint64_t>(bytes[offset++]) << (i * 8);
    }
    
    // Previous hash
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, event.previous_hash.begin());
    offset += 32;
    
    // Data length
    uint32_t data_len = 0;
    for (int i = 0; i < 4; ++i) {
        data_len |= static_cast<uint32_t>(bytes[offset++]) << (i * 8);
    }
    
    // Validate data length
    if (offset + data_len + 64 > bytes.size()) {
        return std::nullopt;
    }
    
    // Data
    event.data.assign(bytes.begin() + offset, bytes.begin() + offset + data_len);
    offset += data_len;
    
    // Signature
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 64, event.signature.begin());
    
    return event;
}

bool LedgerEvent::verify_signature(const PublicKey& public_key) const {
    // Create message to verify (everything except signature)
    std::vector<uint8_t> message;
    message.insert(message.end(), event_id.begin(), event_id.end());
    message.push_back(static_cast<uint8_t>(event_type));
    message.insert(message.end(), source_node.id.begin(), source_node.id.end());
    
    for (int i = 0; i < 8; ++i) {
        message.push_back(static_cast<uint8_t>(timestamp >> (i * 8)));
    }
    for (int i = 0; i < 8; ++i) {
        message.push_back(static_cast<uint8_t>(epoch >> (i * 8)));
    }
    
    message.insert(message.end(), previous_hash.begin(), previous_hash.end());
    message.insert(message.end(), data.begin(), data.end());
    
    return crypto::Ed25519::verify(message, signature, public_key);
}

Hash256 LedgerEvent::compute_hash() const {
    auto bytes = to_bytes();
    return crypto::Blake3::hash(bytes);
}

// KeyIssuanceData methods

std::vector<uint8_t> KeyIssuanceData::to_bytes() const {
    std::vector<uint8_t> data;
    
    data.push_back(static_cast<uint8_t>(key_type));
    
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>(count >> (i * 8)));
    }
    
    data.push_back(static_cast<uint8_t>(method));
    data.insert(data.end(), proof.begin(), proof.end());
    
    return data;
}

std::optional<KeyIssuanceData> KeyIssuanceData::from_bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 1 + 4 + 1 + 32) {
        return std::nullopt;
    }
    
    KeyIssuanceData data;
    size_t offset = 0;
    
    data.key_type = static_cast<core::KeyType>(bytes[offset++]);
    
    data.count = 0;
    for (int i = 0; i < 4; ++i) {
        data.count |= static_cast<uint32_t>(bytes[offset++]) << (i * 8);
    }
    
    data.method = static_cast<IssuanceMethod>(bytes[offset++]);
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.proof.begin());
    
    return data;
}

// NetworkMembershipData, ThingReplicationData, ReputationUpdateData methods
// Similar implementations...

std::vector<uint8_t> NetworkMembershipData::to_bytes() const {
    std::vector<uint8_t> data;
    data.insert(data.end(), network_id.begin(), network_id.end());
    data.insert(data.end(), member_node.id.begin(), member_node.id.end());
    
    uint16_t role_len = static_cast<uint16_t>(role.size());
    data.push_back(static_cast<uint8_t>(role_len & 0xFF));
    data.push_back(static_cast<uint8_t>((role_len >> 8) & 0xFF));
    data.insert(data.end(), role.begin(), role.end());
    
    return data;
}

std::optional<NetworkMembershipData> NetworkMembershipData::from_bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 32 + 32 + 2) {
        return std::nullopt;
    }
    
    NetworkMembershipData data;
    size_t offset = 0;
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.network_id.begin());
    offset += 32;
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.member_node.id.begin());
    offset += 32;
    
    uint16_t role_len = bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8);
    offset += 2;
    
    if (offset + role_len > bytes.size()) {
        return std::nullopt;
    }
    
    data.role = std::string(bytes.begin() + offset, bytes.begin() + offset + role_len);
    
    return data;
}

std::vector<uint8_t> ThingReplicationData::to_bytes() const {
    std::vector<uint8_t> data;
    data.insert(data.end(), content_hash.hash.begin(), content_hash.hash.end());
    data.insert(data.end(), network_id.begin(), network_id.end());
    data.insert(data.end(), hosting_node.id.begin(), hosting_node.id.end());
    
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>(size_bytes >> (i * 8)));
    }
    
    return data;
}

std::optional<ThingReplicationData> ThingReplicationData::from_bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 32 + 32 + 32 + 8) {
        return std::nullopt;
    }
    
    ThingReplicationData data;
    size_t offset = 0;
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.content_hash.hash.begin());
    offset += 32;
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.network_id.begin());
    offset += 32;
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.hosting_node.id.begin());
    offset += 32;
    
    data.size_bytes = 0;
    for (int i = 0; i < 8; ++i) {
        data.size_bytes |= static_cast<uint64_t>(bytes[offset++]) << (i * 8);
    }
    
    return data;
}

std::vector<uint8_t> ReputationUpdateData::to_bytes() const {
    std::vector<uint8_t> data;
    data.insert(data.end(), subject_node.id.begin(), subject_node.id.end());
    
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>(score_delta >> (i * 8)));
    }
    
    uint16_t reason_len = static_cast<uint16_t>(reason.size());
    data.push_back(static_cast<uint8_t>(reason_len & 0xFF));
    data.push_back(static_cast<uint8_t>((reason_len >> 8) & 0xFF));
    data.insert(data.end(), reason.begin(), reason.end());
    
    data.insert(data.end(), evidence.begin(), evidence.end());
    
    return data;
}

std::optional<ReputationUpdateData> ReputationUpdateData::from_bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 32 + 4 + 2 + 32) {
        return std::nullopt;
    }
    
    ReputationUpdateData data;
    size_t offset = 0;
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.subject_node.id.begin());
    offset += 32;
    
    data.score_delta = 0;
    for (int i = 0; i < 4; ++i) {
        data.score_delta |= static_cast<int32_t>(bytes[offset++]) << (i * 8);
    }
    
    uint16_t reason_len = bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8);
    offset += 2;
    
    if (offset + reason_len + 32 > bytes.size()) {
        return std::nullopt;
    }
    
    data.reason = std::string(bytes.begin() + offset, bytes.begin() + offset + reason_len);
    offset += reason_len;
    
    std::copy(bytes.begin() + offset, bytes.begin() + offset + 32, data.evidence.begin());
    
    return data;
}

// LedgerIndex methods

void LedgerIndex::add_event(const LedgerEvent& event) {
    Hash256 event_id = event.event_id;
    
    // Index by node
    events_by_node_[event.source_node].push_back(event_id);
    
    // Index by type
    events_by_type_[event.event_type].push_back(event_id);
    
    // Type-specific indexing
    switch (event.event_type) {
        case EventType::NODE_JOINED:
            node_join_times_[event.source_node] = event.timestamp;
            break;
            
        case EventType::KEY_ISSUED: {
            auto key_data_opt = KeyIssuanceData::from_bytes(event.data);
            if (key_data_opt) {
                auto key = std::make_pair(event.source_node, key_data_opt->key_type);
                key_balances_[key] += key_data_opt->count;
            }
            break;
        }
        
        case EventType::NETWORK_MEMBER_ADDED: {
            auto net_data_opt = NetworkMembershipData::from_bytes(event.data);
            if (net_data_opt) {
                events_by_network_[net_data_opt->network_id].push_back(event_id);
                network_members_[net_data_opt->network_id].insert(net_data_opt->member_node);
            }
            break;
        }
        
        case EventType::THING_REPLICATED: {
            auto thing_data_opt = ThingReplicationData::from_bytes(event.data);
            if (thing_data_opt) {
                events_by_thing_[thing_data_opt->content_hash].push_back(event_id);
                thing_hosts_[thing_data_opt->content_hash].insert(thing_data_opt->hosting_node);
            }
            break;
        }
        
        default:
            break;
    }
}

void LedgerIndex::rebuild_from_events(const std::vector<LedgerEvent>& events) {
    clear();
    for (const auto& event : events) {
        add_event(event);
    }
}

std::vector<Hash256> LedgerIndex::get_events_by_node(const NodeID& node_id) const {
    auto it = events_by_node_.find(node_id);
    if (it == events_by_node_.end()) {
        return {};
    }
    return it->second;
}

std::optional<uint64_t> LedgerIndex::get_node_join_time(const NodeID& node_id) const {
    auto it = node_join_times_.find(node_id);
    if (it == node_join_times_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Hash256> LedgerIndex::get_key_events_by_node(const NodeID& node_id) const {
    std::vector<Hash256> result;
    auto node_events = get_events_by_node(node_id);
    
    // Filter for key events
    // (Would need event lookup to check type - simplified here)
    return result;
}

uint32_t LedgerIndex::get_total_keys_issued(const NodeID& node_id, core::KeyType type) const {
    auto key = std::make_pair(node_id, type);
    auto it = key_balances_.find(key);
    if (it == key_balances_.end()) {
        return 0;
    }
    return it->second;
}

std::vector<Hash256> LedgerIndex::get_network_events(const Hash256& network_id) const {
    auto it = events_by_network_.find(network_id);
    if (it == events_by_network_.end()) {
        return {};
    }
    return it->second;
}

std::vector<NodeID> LedgerIndex::get_network_members(const Hash256& network_id) const {
    auto it = network_members_.find(network_id);
    if (it == network_members_.end()) {
        return {};
    }
    return std::vector<NodeID>(it->second.begin(), it->second.end());
}

std::vector<Hash256> LedgerIndex::get_thing_events(const ContentHash& content_hash) const {
    auto it = events_by_thing_.find(content_hash);
    if (it == events_by_thing_.end()) {
        return {};
    }
    return it->second;
}

std::vector<NodeID> LedgerIndex::get_thing_hosts(const ContentHash& content_hash) const {
    auto it = thing_hosts_.find(content_hash);
    if (it == thing_hosts_.end()) {
        return {};
    }
    return std::vector<NodeID>(it->second.begin(), it->second.end());
}

std::vector<Hash256> LedgerIndex::get_reputation_events(const NodeID& node_id) const {
    // Simplified - would filter by event type
    return get_events_by_node(node_id);
}

void LedgerIndex::clear() {
    events_by_node_.clear();
    events_by_type_.clear();
    events_by_network_.clear();
    events_by_thing_.clear();
    node_join_times_.clear();
    network_members_.clear();
    thing_hosts_.clear();
    key_balances_.clear();
}

// Ledger methods

Ledger::Ledger(const NodeID& local_node_id)
    : local_node_id_(local_node_id),
      event_counter_(0)
{
    // Initialize with genesis event
    latest_hash_ = Hash256{};  // All zeros for genesis
    CASHEW_LOG_INFO("Ledger initialized");
}

void Ledger::set_event_callback(EventCallback callback) {
    event_callback_ = std::move(callback);
    CASHEW_LOG_DEBUG("Ledger event callback registered");
}

LedgerEvent Ledger::create_event(EventType type, const std::vector<uint8_t>& data) {
    LedgerEvent event;
    event.event_type = type;
    event.source_node = local_node_id_;
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    event.timestamp = static_cast<uint64_t>(now_time_t);
    
    event.epoch = current_epoch();
    event.previous_hash = latest_hash_;
    event.data = data;
    
    // Compute event ID
    std::vector<uint8_t> id_data;
    id_data.insert(id_data.end(), local_node_id_.id.begin(), local_node_id_.id.end());
    for (int i = 0; i < 8; ++i) {
        id_data.push_back(static_cast<uint8_t>(event_counter_ >> (i * 8)));
    }
    for (int i = 0; i < 8; ++i) {
        id_data.push_back(static_cast<uint8_t>(event.timestamp >> (i * 8)));
    }
    event.event_id = crypto::Blake3::hash(id_data);
    
    event_counter_++;
    
    // TODO: Sign event with node's private key
    event.signature = Signature{};  // Placeholder
    
    return event;
}

Hash256 Ledger::add_event(const LedgerEvent& event) {
    // Verify event
    if (!verify_event(event)) {
        CASHEW_LOG_ERROR("Failed to verify event");
        return Hash256{};
    }
    
    // Add to storage
    size_t index = events_.size();
    events_.push_back(event);
    event_lookup_[event.event_id] = index;
    
    // Update index
    index_.add_event(event);
    
    // Update chain
    latest_hash_ = event.compute_hash();
    
    // Trigger callback if registered
    if (event_callback_) {
        try {
            event_callback_(event);
        } catch (const std::exception& e) {
            CASHEW_LOG_ERROR("Event callback failed: {}", e.what());
        }
    }
    
    CASHEW_LOG_DEBUG("Added event to ledger (total: {})", events_.size());
    
    return event.event_id;
}

bool Ledger::verify_event(const LedgerEvent& event) const {
    // TODO: Verify signature with node's public key
    // For now, basic checks
    (void)event;
    return true;
}

Hash256 Ledger::record_node_joined(const NodeID& node_id) {
    std::vector<uint8_t> data;
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    
    auto event = create_event(EventType::NODE_JOINED, data);
    return add_event(event);
}

Hash256 Ledger::record_node_left(const NodeID& node_id) {
    std::vector<uint8_t> data;
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    
    auto event = create_event(EventType::NODE_LEFT, data);
    return add_event(event);
}

Hash256 Ledger::record_key_issued(
    core::KeyType key_type,
    uint32_t count,
    IssuanceMethod method,
    const Hash256& proof
) {
    KeyIssuanceData key_data;
    key_data.key_type = key_type;
    key_data.count = count;
    key_data.method = method;
    key_data.proof = proof;
    
    auto event = create_event(EventType::KEY_ISSUED, key_data.to_bytes());
    return add_event(event);
}

Hash256 Ledger::record_key_revoked(
    core::KeyType key_type,
    uint32_t count,
    const std::string& reason
) {
    (void)key_type;
    (void)count;
    (void)reason;
    // TODO: Implement
    return Hash256{};
}

Hash256 Ledger::record_network_created(const Hash256& network_id) {
    std::vector<uint8_t> data;
    data.insert(data.end(), network_id.begin(), network_id.end());
    
    auto event = create_event(EventType::NETWORK_CREATED, data);
    return add_event(event);
}

Hash256 Ledger::record_network_member_added(
    const Hash256& network_id,
    const NodeID& member_node,
    const std::string& role
) {
    NetworkMembershipData net_data;
    net_data.network_id = network_id;
    net_data.member_node = member_node;
    net_data.role = role;
    
    auto event = create_event(EventType::NETWORK_MEMBER_ADDED, net_data.to_bytes());
    return add_event(event);
}

Hash256 Ledger::record_thing_replicated(
    const ContentHash& content_hash,
    const Hash256& network_id,
    const NodeID& hosting_node,
    uint64_t size_bytes
) {
    ThingReplicationData thing_data;
    thing_data.content_hash = content_hash;
    thing_data.network_id = network_id;
    thing_data.hosting_node = hosting_node;
    thing_data.size_bytes = size_bytes;
    
    auto event = create_event(EventType::THING_REPLICATED, thing_data.to_bytes());
    return add_event(event);
}

Hash256 Ledger::record_reputation_update(
    const NodeID& subject_node,
    int32_t score_delta,
    const std::string& reason
) {
    ReputationUpdateData rep_data;
    rep_data.subject_node = subject_node;
    rep_data.score_delta = score_delta;
    rep_data.reason = reason;
    rep_data.evidence = Hash256{};  // Optional
    
    auto event = create_event(EventType::REPUTATION_UPDATED, rep_data.to_bytes());
    return add_event(event);
}

std::optional<LedgerEvent> Ledger::get_event(const Hash256& event_id) const {
    auto it = event_lookup_.find(event_id);
    if (it == event_lookup_.end()) {
        return std::nullopt;
    }
    return events_[it->second];
}

std::vector<LedgerEvent> Ledger::get_events_by_node(const NodeID& node_id) const {
    std::vector<LedgerEvent> result;
    auto event_ids = index_.get_events_by_node(node_id);
    
    for (const auto& event_id : event_ids) {
        auto event_opt = get_event(event_id);
        if (event_opt) {
            result.push_back(*event_opt);
        }
    }
    
    return result;
}

std::vector<LedgerEvent> Ledger::get_events_by_type(EventType type) const {
    std::vector<LedgerEvent> result;
    
    for (const auto& event : events_) {
        if (event.event_type == type) {
            result.push_back(event);
        }
    }
    
    return result;
}

std::vector<LedgerEvent> Ledger::get_recent_events(size_t count) const {
    size_t start = events_.size() > count ? events_.size() - count : 0;
    return std::vector<LedgerEvent>(events_.begin() + start, events_.end());
}

std::vector<LedgerEvent> Ledger::get_all_events() const {
    return events_;
}

bool Ledger::add_external_event(const LedgerEvent& event) {
    // Verify event chain
    if (!verify_event_chain(event)) {
        CASHEW_LOG_WARN("Event chain verification failed");
        return false;
    }
    
    // Check if we already have this event
    if (event_lookup_.find(event.event_id) != event_lookup_.end()) {
        return false;  // Already have it
    }
    
    add_event(event);
    return true;
}

bool Ledger::verify_event_chain(const LedgerEvent& event) const {
    // TODO: Implement proper chain verification
    // For now, just check if previous hash exists
    (void)event;
    return true;
}

uint64_t Ledger::current_epoch() const {
    // Simple epoch calculation (10-minute epochs)
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t) / 600;  // 600 seconds = 10 minutes
}

Hash256 Ledger::get_latest_hash() const {
    return latest_hash_;
}

bool Ledger::save_to_file(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write event count
    uint64_t count = events_.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write each event
    for (const auto& event : events_) {
        auto bytes = event.to_bytes();
        uint32_t size = static_cast<uint32_t>(bytes.size());
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    
    file.close();
    CASHEW_LOG_INFO("Saved ledger to file ({}events)", count);
    return true;
}

bool Ledger::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Clear current state
    events_.clear();
    event_lookup_.clear();
    index_.clear();
    
    // Read event count
    uint64_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    // Read each event
    for (uint64_t i = 0; i < count; ++i) {
        uint32_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        
        std::vector<uint8_t> bytes(size);
        file.read(reinterpret_cast<char*>(bytes.data()), size);
        
        auto event_opt = LedgerEvent::from_bytes(bytes);
        if (event_opt) {
            add_event(*event_opt);
        }
    }
    
    file.close();
    CASHEW_LOG_INFO("Loaded ledger from file ({} events)", events_.size());
    return true;
}

bool Ledger::validate_chain() const {
    for (size_t i = 1; i < events_.size(); ++i) {
        const auto& prev = events_[i - 1];
        const auto& curr = events_[i];
        
        Hash256 expected_prev = prev.compute_hash();
        if (curr.previous_hash != expected_prev) {
            CASHEW_LOG_ERROR("Chain break at event {}", i);
            return false;
        }
    }
    
    return true;
}

std::vector<Hash256> Ledger::detect_conflicts() const {
    // TODO: Implement conflict detection
    // Check for duplicate event IDs, conflicting states, etc.
    return {};
}

// LedgerStatistics methods

std::string LedgerStatistics::to_string() const {
    std::ostringstream oss;
    oss << "Ledger Statistics:\n";
    oss << "  Total events: " << total_events << "\n";
    oss << "  Total nodes: " << total_nodes << "\n";
    oss << "  Total networks: " << total_networks << "\n";
    oss << "  Total Things: " << total_things << "\n";
    oss << "  Keys issued: " << total_keys_issued << "\n";
    oss << "  Keys revoked: " << total_keys_revoked << "\n";
    oss << "  Oldest event: " << oldest_event_timestamp << "\n";
    oss << "  Newest event: " << newest_event_timestamp;
    return oss.str();
}

} // namespace cashew::ledger
