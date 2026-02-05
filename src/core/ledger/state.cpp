#include "core/ledger/state.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <sstream>

namespace cashew::ledger {

// NodeState methods

bool NodeState::has_key_type(core::KeyType type, uint32_t min_count) const {
    auto it = key_balances.find(type);
    if (it == key_balances.end()) {
        return false;
    }
    return it->second >= min_count;
}

bool NodeState::can_host_things() const {
    return is_active && has_key_type(core::KeyType::SERVICE, 1);
}

bool NodeState::can_join_networks() const {
    return is_active && has_key_type(core::KeyType::NETWORK, 1);
}

bool NodeState::can_route() const {
    return is_active && has_key_type(core::KeyType::ROUTING, 1);
}

// NetworkState methods

bool NetworkState::has_member(const NodeID& node_id) const {
    return members.find(node_id) != members.end();
}

std::string NetworkState::get_member_role(const NodeID& node_id) const {
    auto it = member_roles.find(node_id);
    if (it == member_roles.end()) {
        return "UNKNOWN";
    }
    return it->second;
}

// ThingState methods

bool ThingState::is_hosted_by(const NodeID& node_id) const {
    return hosts.find(node_id) != hosts.end();
}

bool ThingState::meets_redundancy_requirements(uint32_t min_redundancy) const {
    return replication_count >= min_redundancy;
}

// StateSnapshot methods

std::string StateSnapshot::to_string() const {
    std::ostringstream oss;
    oss << "State Snapshot (epoch " << epoch << "):\n";
    oss << "  Timestamp: " << timestamp << "\n";
    oss << "  Total nodes: " << total_nodes << " (" << active_nodes << " active)\n";
    oss << "  Total networks: " << total_networks << "\n";
    oss << "  Total Things: " << total_things << "\n";
    oss << "  Total keys issued: " << total_keys_issued;
    return oss.str();
}

// StateManager methods

StateManager::StateManager(Ledger& ledger)
    : ledger_(ledger), last_rebuild_(0)
{
    rebuild_state();
    CASHEW_LOG_INFO("StateManager initialized");
}

void StateManager::rebuild_state() {
    CASHEW_LOG_INFO("Rebuilding state from ledger...");
    
    // Clear current state
    nodes_.clear();
    networks_.clear();
    things_.clear();
    
    // Process all events from ledger
    auto events = ledger_.get_all_events();
    for (const auto& event : events) {
        apply_event(event);
    }
    
    last_rebuild_ = current_timestamp();
    
    CASHEW_LOG_INFO("State rebuilt: {} nodes, {} networks, {} Things",
                    nodes_.size(), networks_.size(), things_.size());
}

void StateManager::apply_event(const LedgerEvent& event) {
    switch (event.event_type) {
        case EventType::NODE_JOINED:
            apply_node_joined(event);
            break;
        case EventType::NODE_LEFT:
            apply_node_left(event);
            break;
        case EventType::KEY_ISSUED:
            apply_key_issued(event);
            break;
        case EventType::KEY_REVOKED:
            apply_key_revoked(event);
            break;
        case EventType::NETWORK_CREATED:
            apply_network_created(event);
            break;
        case EventType::NETWORK_MEMBER_ADDED:
            apply_network_member_added(event);
            break;
        case EventType::NETWORK_MEMBER_REMOVED:
            apply_network_member_removed(event);
            break;
        case EventType::THING_REPLICATED:
            apply_thing_replicated(event);
            break;
        case EventType::THING_REMOVED:
            apply_thing_removed(event);
            break;
        case EventType::REPUTATION_UPDATED:
            apply_reputation_updated(event);
            break;
        case EventType::POW_SOLUTION_SUBMITTED:
            apply_pow_solution(event);
            break;
        case EventType::POSTAKE_CONTRIBUTION:
            apply_postake_contribution(event);
            break;
        default:
            break;
    }
}

void StateManager::apply_node_joined(const LedgerEvent& event) {
    NodeState state;
    state.node_id = event.source_node;
    state.joined_at = event.timestamp;
    state.is_active = true;
    
    nodes_[event.source_node] = state;
    CASHEW_LOG_DEBUG("Node joined state");
}

void StateManager::apply_node_left(const LedgerEvent& event) {
    auto it = nodes_.find(event.source_node);
    if (it != nodes_.end()) {
        it->second.is_active = false;
        CASHEW_LOG_DEBUG("Node left state");
    }
}

void StateManager::apply_key_issued(const LedgerEvent& event) {
    auto key_data_opt = KeyIssuanceData::from_bytes(event.data);
    if (!key_data_opt) {
        return;
    }
    
    auto& node_state = nodes_[event.source_node];
    node_state.key_balances[key_data_opt->key_type] += key_data_opt->count;
    
    CASHEW_LOG_DEBUG("Keys issued to node: {} x{}", 
                    static_cast<int>(key_data_opt->key_type), 
                    key_data_opt->count);
}

void StateManager::apply_key_revoked(const LedgerEvent& event) {
    // TODO: Implement key revocation
    (void)event;
}

void StateManager::apply_network_created(const LedgerEvent& event) {
    if (event.data.size() < 32) {
        return;
    }
    
    Hash256 network_id;
    std::copy(event.data.begin(), event.data.begin() + 32, network_id.begin());
    
    NetworkState state;
    state.network_id = network_id;
    state.created_at = event.timestamp;
    state.is_active = true;
    
    networks_[network_id] = state;
    CASHEW_LOG_DEBUG("Network created in state");
}

void StateManager::apply_network_member_added(const LedgerEvent& event) {
    auto net_data_opt = NetworkMembershipData::from_bytes(event.data);
    if (!net_data_opt) {
        return;
    }
    
    auto& network_state = networks_[net_data_opt->network_id];
    network_state.members.insert(net_data_opt->member_node);
    network_state.member_roles[net_data_opt->member_node] = net_data_opt->role;
    
    // Update node state
    auto& node_state = nodes_[net_data_opt->member_node];
    node_state.networks.insert(net_data_opt->network_id);
    
    CASHEW_LOG_DEBUG("Network member added to state");
}

void StateManager::apply_network_member_removed(const LedgerEvent& event) {
    // TODO: Implement member removal
    (void)event;
}

void StateManager::apply_thing_replicated(const LedgerEvent& event) {
    auto thing_data_opt = ThingReplicationData::from_bytes(event.data);
    if (!thing_data_opt) {
        return;
    }
    
    auto& thing_state = things_[thing_data_opt->content_hash];
    thing_state.content_hash = thing_data_opt->content_hash;
    thing_state.created_at = event.timestamp;
    thing_state.is_available = true;
    thing_state.hosts.insert(thing_data_opt->hosting_node);
    thing_state.networks.insert(thing_data_opt->network_id);
    thing_state.total_size_bytes = thing_data_opt->size_bytes;
    thing_state.replication_count = static_cast<uint32_t>(thing_state.hosts.size());
    
    // Update node state
    auto& node_state = nodes_[thing_data_opt->hosting_node];
    node_state.hosted_things.insert(thing_data_opt->content_hash);
    
    CASHEW_LOG_DEBUG("Thing replicated in state (hosts: {})", thing_state.replication_count);
}

void StateManager::apply_thing_removed(const LedgerEvent& event) {
    // TODO: Implement thing removal
    (void)event;
}

void StateManager::apply_reputation_updated(const LedgerEvent& event) {
    auto rep_data_opt = ReputationUpdateData::from_bytes(event.data);
    if (!rep_data_opt) {
        return;
    }
    
    auto& node_state = nodes_[rep_data_opt->subject_node];
    node_state.reputation_score += rep_data_opt->score_delta;
    
    CASHEW_LOG_DEBUG("Reputation updated: {} (delta: {})", 
                    node_state.reputation_score, rep_data_opt->score_delta);
}

void StateManager::apply_pow_solution(const LedgerEvent& event) {
    auto& node_state = nodes_[event.source_node];
    node_state.pow_solutions++;
}

void StateManager::apply_postake_contribution(const LedgerEvent& event) {
    auto& node_state = nodes_[event.source_node];
    node_state.postake_contributions++;
}

std::optional<NodeState> StateManager::get_node_state(const NodeID& node_id) const {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<NodeState> StateManager::get_all_active_nodes() const {
    std::vector<NodeState> result;
    result.reserve(nodes_.size());
    
    for (const auto& [node_id, state] : nodes_) {
        if (state.is_active) {
            result.push_back(state);
        }
    }
    
    return result;
}

std::vector<NodeID> StateManager::get_nodes_with_key_type(core::KeyType type, uint32_t min_count) const {
    std::vector<NodeID> result;
    
    for (const auto& [node_id, state] : nodes_) {
        if (state.is_active && state.has_key_type(type, min_count)) {
            result.push_back(node_id);
        }
    }
    
    return result;
}

bool StateManager::is_node_active(const NodeID& node_id) const {
    auto it = nodes_.find(node_id);
    return it != nodes_.end() && it->second.is_active;
}

uint32_t StateManager::get_node_key_balance(const NodeID& node_id, core::KeyType type) const {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return 0;
    }
    
    auto key_it = it->second.key_balances.find(type);
    if (key_it == it->second.key_balances.end()) {
        return 0;
    }
    
    return key_it->second;
}

int32_t StateManager::get_node_reputation(const NodeID& node_id) const {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return 0;
    }
    return it->second.reputation_score;
}

std::optional<NetworkState> StateManager::get_network_state(const Hash256& network_id) const {
    auto it = networks_.find(network_id);
    if (it == networks_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<NetworkState> StateManager::get_all_active_networks() const {
    std::vector<NetworkState> result;
    result.reserve(networks_.size());
    
    for (const auto& [network_id, state] : networks_) {
        if (state.is_active) {
            result.push_back(state);
        }
    }
    
    return result;
}

std::vector<Hash256> StateManager::get_networks_for_node(const NodeID& node_id) const {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return {};
    }
    
    return std::vector<Hash256>(it->second.networks.begin(), it->second.networks.end());
}

std::vector<Hash256> StateManager::get_networks_hosting_thing(const ContentHash& content_hash) const {
    auto it = things_.find(content_hash);
    if (it == things_.end()) {
        return {};
    }
    
    return std::vector<Hash256>(it->second.networks.begin(), it->second.networks.end());
}

bool StateManager::is_network_active(const Hash256& network_id) const {
    auto it = networks_.find(network_id);
    return it != networks_.end() && it->second.is_active;
}

bool StateManager::is_node_in_network(const NodeID& node_id, const Hash256& network_id) const {
    auto it = networks_.find(network_id);
    if (it == networks_.end()) {
        return false;
    }
    return it->second.has_member(node_id);
}

std::optional<ThingState> StateManager::get_thing_state(const ContentHash& content_hash) const {
    auto it = things_.find(content_hash);
    if (it == things_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<ThingState> StateManager::get_all_available_things() const {
    std::vector<ThingState> result;
    result.reserve(things_.size());
    
    for (const auto& [content_hash, state] : things_) {
        if (state.is_available) {
            result.push_back(state);
        }
    }
    
    return result;
}

std::vector<NodeID> StateManager::get_thing_hosts(const ContentHash& content_hash) const {
    auto it = things_.find(content_hash);
    if (it == things_.end()) {
        return {};
    }
    
    return std::vector<NodeID>(it->second.hosts.begin(), it->second.hosts.end());
}

bool StateManager::is_thing_available(const ContentHash& content_hash) const {
    auto it = things_.find(content_hash);
    return it != things_.end() && it->second.is_available;
}

uint32_t StateManager::get_thing_replication_count(const ContentHash& content_hash) const {
    auto it = things_.find(content_hash);
    if (it == things_.end()) {
        return 0;
    }
    return it->second.replication_count;
}

bool StateManager::can_node_host_things(const NodeID& node_id) const {
    auto it = nodes_.find(node_id);
    return it != nodes_.end() && it->second.can_host_things();
}

bool StateManager::can_node_join_networks(const NodeID& node_id) const {
    auto it = nodes_.find(node_id);
    return it != nodes_.end() && it->second.can_join_networks();
}

bool StateManager::can_node_route_traffic(const NodeID& node_id) const {
    auto it = nodes_.find(node_id);
    return it != nodes_.end() && it->second.can_route();
}

bool StateManager::can_node_post_content(const NodeID& node_id) const {
    // Require at least one key of any type to post (anti-bot)
    auto it = nodes_.find(node_id);
    if (it == nodes_.end() || !it->second.is_active) {
        return false;
    }
    
    // Check if node has any keys
    for (const auto& [key_type, count] : it->second.key_balances) {
        if (count > 0) {
            return true;
        }
    }
    
    return false;
}

StateSnapshot StateManager::get_snapshot() const {
    StateSnapshot snapshot;
    snapshot.timestamp = current_timestamp();
    snapshot.epoch = ledger_.current_epoch();
    snapshot.ledger_hash = ledger_.get_latest_hash();
    
    snapshot.total_nodes = nodes_.size();
    snapshot.active_nodes = 0;
    for (const auto& [node_id, state] : nodes_) {
        if (state.is_active) {
            snapshot.active_nodes++;
        }
    }
    
    snapshot.total_networks = networks_.size();
    snapshot.total_things = things_.size();
    
    // Count total keys
    snapshot.total_keys_issued = 0;
    for (const auto& [node_id, state] : nodes_) {
        for (const auto& [key_type, count] : state.key_balances) {
            snapshot.total_keys_issued += count;
        }
    }
    
    return snapshot;
}

void StateManager::update_node_activity() {
    // TODO: Mark nodes as inactive based on last activity
    // This would integrate with PoStake uptime tracking
}

void StateManager::cleanup_stale_state() {
    // Remove inactive nodes after threshold
    std::vector<NodeID> to_remove;
    
    uint64_t current_time = current_timestamp();
    static constexpr uint64_t INACTIVE_THRESHOLD = 86400 * 30;  // 30 days
    
    for (const auto& [node_id, state] : nodes_) {
        if (!state.is_active) {
            uint64_t inactive_time = current_time - state.joined_at;
            if (inactive_time > INACTIVE_THRESHOLD) {
                to_remove.push_back(node_id);
            }
        }
    }
    
    for (const auto& node_id : to_remove) {
        nodes_.erase(node_id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_INFO("Cleaned up {} inactive nodes from state", to_remove.size());
    }
}

uint64_t StateManager::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

} // namespace cashew::ledger
