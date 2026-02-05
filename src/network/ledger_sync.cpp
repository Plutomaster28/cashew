#include "network/ledger_sync.hpp"
#include "crypto/blake3.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace cashew::network {

// LedgerSyncMessage methods

std::vector<uint8_t> LedgerSyncMessage::serialize() const {
    std::vector<uint8_t> data;
    
    // Type (1 byte)
    data.push_back(static_cast<uint8_t>(type));
    
    // Epoch range (16 bytes)
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(start_epoch >> (i * 8)));
    }
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(end_epoch >> (i * 8)));
    }
    
    // Ledger hash (32 bytes)
    data.insert(data.end(), ledger_hash.begin(), ledger_hash.end());
    
    // Number of events (4 bytes)
    uint32_t event_count = static_cast<uint32_t>(events.size());
    for (int i = 0; i < 4; i++) {
        data.push_back(static_cast<uint8_t>(event_count >> (i * 8)));
    }
    
    // Events (serialized)
    for (const auto& event : events) {
        auto event_data = event.to_bytes();
        
        // Event size (4 bytes)
        uint32_t event_size = static_cast<uint32_t>(event_data.size());
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(event_size >> (i * 8)));
        }
        
        // Event data
        data.insert(data.end(), event_data.begin(), event_data.end());
    }
    
    return data;
}

std::optional<LedgerSyncMessage> LedgerSyncMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 57) {  // Minimum size: 1 + 16 + 32 + 4 + 4
        return std::nullopt;
    }
    
    LedgerSyncMessage msg;
    size_t offset = 0;
    
    // Type
    msg.type = static_cast<Type>(data[offset++]);
    
    // Epoch range
    msg.start_epoch = 0;
    for (int i = 0; i < 8; i++) {
        msg.start_epoch |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    msg.end_epoch = 0;
    for (int i = 0; i < 8; i++) {
        msg.end_epoch |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Ledger hash
    std::copy(data.begin() + offset, data.begin() + offset + 32, msg.ledger_hash.begin());
    offset += 32;
    
    // Number of events
    uint32_t event_count = 0;
    for (int i = 0; i < 4; i++) {
        event_count |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    // Events
    for (uint32_t i = 0; i < event_count; i++) {
        if (offset + 4 > data.size()) {
            return std::nullopt;
        }
        
        // Event size
        uint32_t event_size = 0;
        for (int j = 0; j < 4; j++) {
            event_size |= static_cast<uint32_t>(data[offset++]) << (j * 8);
        }
        
        if (offset + event_size > data.size()) {
            return std::nullopt;
        }
        
        // Event data
        std::vector<uint8_t> event_data(data.begin() + offset, data.begin() + offset + event_size);
        offset += event_size;
        
        auto event = ledger::LedgerEvent::from_bytes(event_data);
        if (!event) {
            return std::nullopt;
        }
        
        msg.events.push_back(*event);
    }
    
    return msg;
}

// LedgerGossipBridge methods

LedgerGossipBridge::LedgerGossipBridge(ledger::Ledger& ledger,
                                       ledger::StateManager& state_manager,
                                       GossipProtocol& gossip)
    : ledger_(ledger), state_manager_(state_manager), gossip_(gossip),
      events_received_(0), events_sent_(0), sync_requests_(0)
{
    CASHEW_LOG_INFO("LedgerGossipBridge initialized");
}

void LedgerGossipBridge::broadcast_event(const ledger::LedgerEvent& event) {
    LedgerSyncMessage msg;
    msg.type = LedgerSyncMessage::Type::EVENT_BROADCAST;
    msg.events.push_back(event);
    msg.start_epoch = event.epoch;
    msg.end_epoch = event.epoch;
    msg.ledger_hash = ledger_.get_latest_hash();
    
    auto serialized = msg.serialize();
    
    // Create gossip message
    GossipMessage gossip_msg;
    gossip_msg.type = GossipMessageType::NETWORK_STATE_UPDATE;
    gossip_msg.message_id = event.event_id;
    gossip_msg.payload = serialized;
    gossip_msg.timestamp = current_timestamp();
    gossip_msg.hop_count = 0;
    
    gossip_.broadcast_message(gossip_msg);
    
    events_sent_++;
    CASHEW_LOG_DEBUG("Broadcast ledger event (epoch {})", event.epoch);
}

void LedgerGossipBridge::broadcast_checkpoint(uint64_t epoch) {
    LedgerSyncMessage msg;
    msg.type = LedgerSyncMessage::Type::CHECKPOINT;
    msg.start_epoch = epoch;
    msg.end_epoch = epoch;
    msg.ledger_hash = ledger_.get_latest_hash();
    
    auto serialized = msg.serialize();
    
    GossipMessage gossip_msg;
    gossip_msg.type = GossipMessageType::NETWORK_STATE_UPDATE;
    gossip_msg.message_id = msg.ledger_hash;
    gossip_msg.payload = serialized;
    gossip_msg.timestamp = current_timestamp();
    gossip_msg.hop_count = 0;
    
    gossip_.broadcast_message(gossip_msg);
    
    CASHEW_LOG_INFO("Broadcast ledger checkpoint (epoch {}, hash {})", 
                   epoch, crypto::Blake3::hash_to_hex(msg.ledger_hash).substr(0, 16));
}

void LedgerGossipBridge::request_sync(const NodeID& /* peer_id */, uint64_t start_epoch, uint64_t end_epoch) {
    LedgerSyncMessage msg;
    msg.type = LedgerSyncMessage::Type::SYNC_REQUEST;
    msg.start_epoch = start_epoch;
    msg.end_epoch = end_epoch;
    msg.ledger_hash = ledger_.get_latest_hash();
    
    auto serialized = msg.serialize();
    
    GossipMessage gossip_msg;
    gossip_msg.type = GossipMessageType::NETWORK_STATE_UPDATE;
    
    // Create unique message ID for sync request
    std::vector<uint8_t> id_data;
    for (int i = 0; i < 8; i++) {
        id_data.push_back(static_cast<uint8_t>(start_epoch >> (i * 8)));
        id_data.push_back(static_cast<uint8_t>(end_epoch >> (i * 8)));
    }
    gossip_msg.message_id = crypto::Blake3::hash(id_data);
    
    gossip_msg.payload = serialized;
    gossip_msg.timestamp = current_timestamp();
    gossip_msg.hop_count = 0;
    
    // TODO: Send to specific peer (need peer-specific sending in GossipProtocol)
    gossip_.broadcast_message(gossip_msg);
    
    sync_requests_++;
    CASHEW_LOG_DEBUG("Requested sync from epoch {} to {}", start_epoch, end_epoch);
}

void LedgerGossipBridge::handle_sync_request(const NodeID& /* peer_id */, uint64_t start_epoch, uint64_t end_epoch) {
    // Get events in range (filter all events by epoch)
    auto all_events = ledger_.get_all_events();
    std::vector<ledger::LedgerEvent> events;
    for (const auto& evt : all_events) {
        if (evt.epoch >= start_epoch && evt.epoch <= end_epoch) {
            events.push_back(evt);
        }
    }
    
    LedgerSyncMessage msg;
    msg.type = LedgerSyncMessage::Type::SYNC_RESPONSE;
    msg.events = events;
    msg.start_epoch = start_epoch;
    msg.end_epoch = end_epoch;
    msg.ledger_hash = ledger_.get_latest_hash();
    
    auto serialized = msg.serialize();
    
    GossipMessage gossip_msg;
    gossip_msg.type = GossipMessageType::NETWORK_STATE_UPDATE;
    gossip_msg.message_id = crypto::Blake3::hash(serialized);
    gossip_msg.payload = serialized;
    gossip_msg.timestamp = current_timestamp();
    gossip_msg.hop_count = 0;
    
    // TODO: Send to specific peer
    gossip_.broadcast_message(gossip_msg);
    
    CASHEW_LOG_DEBUG("Sent sync response: {} events (epoch {} to {})", 
                    events.size(), start_epoch, end_epoch);
}

void LedgerGossipBridge::handle_sync_response(const NodeID& peer_id, 
                                               const std::vector<ledger::LedgerEvent>& events) {
    for (const auto& event : events) {
        process_received_event(event);
    }
    
    if (!events.empty()) {
        update_peer_sync_state(peer_id, events.back().epoch, events.back().event_id);
    }
    
    CASHEW_LOG_INFO("Processed sync response: {} events", events.size());
}

void LedgerGossipBridge::handle_gossip_message(const NodeID& source, const GossipMessage& message) {
    // Deserialize ledger sync message
    auto sync_msg = LedgerSyncMessage::deserialize(message.payload);
    if (!sync_msg) {
        CASHEW_LOG_WARN("Failed to deserialize ledger sync message");
        return;
    }
    
    switch (sync_msg->type) {
        case LedgerSyncMessage::Type::EVENT_BROADCAST:
            for (const auto& event : sync_msg->events) {
                process_received_event(event);
            }
            break;
            
        case LedgerSyncMessage::Type::SYNC_REQUEST:
            handle_sync_request(source, sync_msg->start_epoch, sync_msg->end_epoch);
            break;
            
        case LedgerSyncMessage::Type::SYNC_RESPONSE:
            handle_sync_response(source, sync_msg->events);
            break;
            
        case LedgerSyncMessage::Type::CHECKPOINT:
            update_peer_sync_state(source, sync_msg->start_epoch, sync_msg->ledger_hash);
            break;
    }
}

void LedgerGossipBridge::sync_with_network() {
    uint64_t current_epoch = ledger_.current_epoch();
    
    // Find highest known epoch from peers
    uint64_t max_peer_epoch = current_epoch;
    for (const auto& [peer_id, state] : peer_sync_states_) {
        if (state.last_synced_epoch > max_peer_epoch) {
            max_peer_epoch = state.last_synced_epoch;
        }
    }
    
    // Request missing epochs
    if (max_peer_epoch > current_epoch) {
        // Pick a random peer with higher epoch
        std::vector<NodeID> candidates;
        for (const auto& [peer_id, state] : peer_sync_states_) {
            if (state.last_synced_epoch > current_epoch) {
                candidates.push_back(peer_id);
            }
        }
        
        if (!candidates.empty()) {
            // Request from first candidate (TODO: better peer selection)
            request_sync(candidates[0], current_epoch + 1, max_peer_epoch);
        }
    }
}

void LedgerGossipBridge::validate_consistency() {
    // Check for conflicts between our ledger and peer states
    uint64_t current_epoch = ledger_.current_epoch();
    Hash256 our_hash = ledger_.get_latest_hash();
    
    for (const auto& [peer_id, state] : peer_sync_states_) {
        if (state.last_synced_epoch == current_epoch && state.last_known_hash != our_hash) {
            CASHEW_LOG_WARN("Ledger hash mismatch with peer at epoch {}", current_epoch);
            // TODO: Conflict resolution
        }
    }
}

void LedgerGossipBridge::cleanup_sync_state() {
    uint64_t current_time = current_timestamp();
    static constexpr uint64_t STALE_THRESHOLD = 300;  // 5 minutes
    
    std::vector<NodeID> to_remove;
    for (const auto& [peer_id, state] : peer_sync_states_) {
        if (current_time - state.last_sync_time > STALE_THRESHOLD) {
            to_remove.push_back(peer_id);
        }
    }
    
    for (const auto& peer_id : to_remove) {
        peer_sync_states_.erase(peer_id);
    }
}

bool LedgerGossipBridge::is_synced() const {
    if (peer_sync_states_.empty()) {
        return false;  // No peers to sync with
    }
    
    uint64_t current_epoch = ledger_.current_epoch();
    
    // Check if we're caught up with majority of peers
    size_t synced_count = 0;
    for (const auto& [peer_id, state] : peer_sync_states_) {
        if (state.last_synced_epoch <= current_epoch + 1) {
            synced_count++;
        }
    }
    
    return synced_count >= peer_sync_states_.size() / 2;
}

uint64_t LedgerGossipBridge::get_sync_epoch() const {
    return ledger_.current_epoch();
}

std::vector<NodeID> LedgerGossipBridge::get_synced_peers() const {
    std::vector<NodeID> result;
    uint64_t current_epoch = ledger_.current_epoch();
    
    for (const auto& [peer_id, state] : peer_sync_states_) {
        if (state.last_synced_epoch >= current_epoch - 1) {
            result.push_back(peer_id);
        }
    }
    
    return result;
}

void LedgerGossipBridge::process_received_event(const ledger::LedgerEvent& event) {
    // Check if already seen
    if (seen_event_ids_.find(event.event_id) != seen_event_ids_.end()) {
        return;
    }
    
    // Validate event chain
    if (!validate_event_chain(event)) {
        CASHEW_LOG_WARN("Invalid event chain, rejecting event");
        return;
    }
    
    // Add to ledger using public method
    if (ledger_.add_external_event(event)) {
        seen_event_ids_.insert(event.event_id);
        events_received_++;
        
        // Apply to state manager
        state_manager_.apply_event(event);
        
        CASHEW_LOG_DEBUG("Added event from network (epoch {})", event.epoch);
    }
}

bool LedgerGossipBridge::validate_event_chain(const ledger::LedgerEvent& event) const {
    // If this is the first event or for a future epoch, accept it
    if (event.epoch > ledger_.current_epoch()) {
        return true;  // TODO: More sophisticated validation
    }
    
    // Verify chain continuity using public method
    return ledger_.verify_event_chain(event);
}

void LedgerGossipBridge::update_peer_sync_state(const NodeID& peer_id, uint64_t epoch, const Hash256& hash) {
    auto& state = peer_sync_states_[peer_id];
    state.peer_id = peer_id;
    state.last_synced_epoch = epoch;
    state.last_known_hash = hash;
    state.last_sync_time = current_timestamp();
}

uint64_t LedgerGossipBridge::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

// LedgerSyncScheduler methods

LedgerSyncScheduler::LedgerSyncScheduler(LedgerGossipBridge& bridge)
    : bridge_(bridge), running_(false),
      last_sync_time_(0), last_checkpoint_epoch_(0), last_validation_time_(0)
{
}

void LedgerSyncScheduler::start() {
    running_ = true;
    last_sync_time_ = current_timestamp();
    last_validation_time_ = current_timestamp();
    CASHEW_LOG_INFO("LedgerSyncScheduler started");
}

void LedgerSyncScheduler::stop() {
    running_ = false;
    CASHEW_LOG_INFO("LedgerSyncScheduler stopped");
}

void LedgerSyncScheduler::tick() {
    if (!running_) {
        return;
    }
    
    uint64_t current = current_timestamp();
    
    // Periodic sync
    if (current - last_sync_time_ >= SYNC_INTERVAL) {
        bridge_.sync_with_network();
        last_sync_time_ = current;
    }
    
    // Periodic checkpoint
    uint64_t current_epoch = bridge_.get_sync_epoch();
    if (current_epoch >= last_checkpoint_epoch_ + CHECKPOINT_INTERVAL) {
        bridge_.broadcast_checkpoint(current_epoch);
        last_checkpoint_epoch_ = current_epoch;
    }
    
    // Periodic validation
    if (current - last_validation_time_ >= VALIDATION_INTERVAL) {
        bridge_.validate_consistency();
        bridge_.cleanup_sync_state();
        last_validation_time_ = current;
    }
}

uint64_t LedgerSyncScheduler::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

} // namespace cashew::network
