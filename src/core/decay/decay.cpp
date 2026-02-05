#include "core/decay/decay.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace cashew::decay {

// NodeActivity methods

bool NodeActivity::is_inactive(uint64_t threshold) const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current = static_cast<uint64_t>(now_time_t);
    
    return (current - last_seen) > threshold;
}

bool NodeActivity::has_used_key_type(core::KeyType type, uint64_t threshold) const {
    auto it = last_use_by_type.find(type);
    if (it == last_use_by_type.end()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current = static_cast<uint64_t>(now_time_t);
    
    return (current - it->second) <= threshold;
}

// ThingActivity methods

bool ThingActivity::is_inactive(uint64_t threshold) const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current = static_cast<uint64_t>(now_time_t);
    
    return (current - last_accessed) > threshold;
}

bool ThingActivity::meets_redundancy(uint32_t min_hosts) const {
    return current_hosts.size() >= min_hosts;
}

// DecayScheduler methods

DecayScheduler::DecayScheduler(ledger::StateManager& state_manager)
    : state_manager_(state_manager)
{
    initialize_default_policies();
    CASHEW_LOG_INFO("DecayScheduler initialized");
}

void DecayScheduler::set_key_policy(core::KeyType key_type, const DecayPolicy& policy) {
    key_policies_[key_type] = policy;
}

DecayPolicy DecayScheduler::get_key_policy(core::KeyType key_type) const {
    auto it = key_policies_.find(key_type);
    if (it == key_policies_.end()) {
        return DecayPolicy{};
    }
    return it->second;
}

void DecayScheduler::set_thing_policy(const ThingDecayPolicy& policy) {
    thing_policy_ = policy;
}

ThingDecayPolicy DecayScheduler::get_thing_policy() const {
    return thing_policy_;
}

void DecayScheduler::record_node_activity(const NodeID& node_id) {
    auto& activity = node_activities_[node_id];
    activity.node_id = node_id;
    activity.last_seen = current_timestamp();
}

void DecayScheduler::record_key_use(const NodeID& node_id, core::KeyType key_type) {
    auto& activity = node_activities_[node_id];
    activity.node_id = node_id;
    
    uint64_t current = current_timestamp();
    activity.last_key_use = current;
    activity.last_use_by_type[key_type] = current;
    activity.actions_this_epoch[key_type]++;
}

void DecayScheduler::record_thing_access(const ContentHash& content_hash) {
    auto& activity = thing_activities_[content_hash];
    activity.content_hash = content_hash;
    activity.last_accessed = current_timestamp();
    activity.access_count++;
    
    // Update hosts from state
    update_thing_activity_from_state(content_hash);
}

void DecayScheduler::process_epoch(uint64_t epoch) {
    CASHEW_LOG_INFO("Processing decay for epoch {}", epoch);
    
    // Check key decay
    auto key_decays = check_key_decay(epoch);
    for (const auto& decay : key_decays) {
        apply_key_decay(decay);
    }
    
    // Check Thing decay
    auto thing_decays = check_thing_decay();
    for (const auto& decay : thing_decays) {
        apply_thing_decay(decay);
    }
    
    // Reset epoch counters
    for (auto& [node_id, activity] : node_activities_) {
        activity.actions_this_epoch.clear();
    }
    
    CASHEW_LOG_INFO("Decay epoch {} complete: {} keys decayed, {} Things removed",
                   epoch, key_decays.size(), thing_decays.size());
}

std::vector<KeyDecayEvent> DecayScheduler::check_key_decay(uint64_t epoch) {
    std::vector<KeyDecayEvent> decays;
    
    auto active_nodes = state_manager_.get_all_active_nodes();
    
    for (const auto& node_state : active_nodes) {
        // Update activity from state if not tracked
        if (node_activities_.find(node_state.node_id) == node_activities_.end()) {
            update_node_activity_from_state(node_state.node_id);
        }
        
        // Check each key type
        for (const auto& [key_type, count] : node_state.key_balances) {
            if (count == 0) continue;
            
            auto policy = get_key_policy(key_type);
            DecayReason reason;
            
            if (should_decay_key(node_state.node_id, key_type, policy, reason)) {
                KeyDecayEvent event;
                event.node_id = node_state.node_id;
                event.key_type = key_type;
                event.keys_decayed = 1;  // Decay one key at a time
                event.reason = reason;
                event.decayed_at = current_timestamp();
                event.epoch = epoch;
                
                decays.push_back(event);
            }
        }
    }
    
    if (!decays.empty()) {
        key_decay_by_epoch_[epoch] = decays;
    }
    
    return decays;
}

std::vector<ThingDecayEvent> DecayScheduler::check_thing_decay() {
    std::vector<ThingDecayEvent> decays;
    
    auto available_things = state_manager_.get_all_available_things();
    
    for (const auto& thing_state : available_things) {
        // Update activity from state if not tracked
        if (thing_activities_.find(thing_state.content_hash) == thing_activities_.end()) {
            update_thing_activity_from_state(thing_state.content_hash);
        }
        
        DecayReason reason;
        if (should_decay_thing(thing_state.content_hash, thing_policy_, reason)) {
            ThingDecayEvent event;
            event.content_hash = thing_state.content_hash;
            event.hosts_removed = std::vector<NodeID>(thing_state.hosts.begin(), thing_state.hosts.end());
            event.reason = reason;
            event.decayed_at = current_timestamp();
            
            decays.push_back(event);
        }
    }
    
    return decays;
}

void DecayScheduler::apply_key_decay(const KeyDecayEvent& event) {
    // TODO: Actually remove keys through ledger
    CASHEW_LOG_INFO("Decayed {} {} keys (reason: {})",
                   event.keys_decayed, static_cast<int>(event.key_type), 
                   static_cast<int>(event.reason));
    
    // Record in history
    if (thing_decay_history_.size() < key_decay_by_epoch_.size()) {
        thing_decay_history_.reserve(key_decay_by_epoch_.size());
    }
}

void DecayScheduler::apply_thing_decay(const ThingDecayEvent& event) {
    // TODO: Actually remove Thing through ledger
    CASHEW_LOG_INFO("Removed Thing from {} hosts (reason: {})",
                   event.hosts_removed.size(), static_cast<int>(event.reason));
    
    thing_decay_history_.push_back(event);
}

std::vector<KeyDecayEvent> DecayScheduler::get_key_decay_history(const NodeID& node_id) const {
    std::vector<KeyDecayEvent> result;
    
    for (const auto& [epoch, events] : key_decay_by_epoch_) {
        for (const auto& event : events) {
            if (event.node_id == node_id) {
                result.push_back(event);
            }
        }
    }
    
    return result;
}

std::vector<ThingDecayEvent> DecayScheduler::get_thing_decay_history() const {
    return thing_decay_history_;
}

uint32_t DecayScheduler::get_total_keys_decayed(uint64_t epoch) const {
    auto it = key_decay_by_epoch_.find(epoch);
    if (it == key_decay_by_epoch_.end()) {
        return 0;
    }
    
    uint32_t total = 0;
    for (const auto& event : it->second) {
        total += event.keys_decayed;
    }
    
    return total;
}

uint32_t DecayScheduler::get_total_things_decayed() const {
    return static_cast<uint32_t>(thing_decay_history_.size());
}

std::map<DecayReason, uint32_t> DecayScheduler::get_decay_reasons_breakdown() const {
    std::map<DecayReason, uint32_t> breakdown;
    
    // Count key decays
    for (const auto& [epoch, events] : key_decay_by_epoch_) {
        for (const auto& event : events) {
            breakdown[event.reason] += event.keys_decayed;
        }
    }
    
    // Count thing decays
    for (const auto& event : thing_decay_history_) {
        breakdown[event.reason]++;
    }
    
    return breakdown;
}

void DecayScheduler::force_decay_check() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_time = static_cast<uint64_t>(now_time_t);
    
    // Use current time as pseudo-epoch
    uint64_t pseudo_epoch = current_time / (10 * 60);  // 10-minute epochs
    process_epoch(pseudo_epoch);
}

void DecayScheduler::cleanup_inactive_nodes(uint64_t threshold) {
    std::vector<NodeID> to_remove;
    uint64_t current = current_timestamp();
    
    for (const auto& [node_id, activity] : node_activities_) {
        if (current - activity.last_seen > threshold) {
            to_remove.push_back(node_id);
        }
    }
    
    for (const auto& node_id : to_remove) {
        node_activities_.erase(node_id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_INFO("Cleaned up {} inactive nodes", to_remove.size());
    }
}

void DecayScheduler::initialize_default_policies() {
    // IDENTITY keys - never decay (core identity)
    {
        DecayPolicy policy;
        policy.key_type = core::KeyType::IDENTITY;
        policy.max_age_seconds = 0;  // Never expire
        policy.requires_activity = false;
        key_policies_[core::KeyType::IDENTITY] = policy;
    }
    
    // NODE keys - 30 day expiry, requires activity
    {
        DecayPolicy policy;
        policy.key_type = core::KeyType::NODE;
        policy.max_age_seconds = 30 * 24 * 60 * 60;
        policy.inactivity_threshold = 7 * 24 * 60 * 60;
        policy.requires_activity = true;
        policy.min_actions_per_epoch = 1;
        key_policies_[core::KeyType::NODE] = policy;
    }
    
    // SERVICE keys - requires hosting activity
    {
        DecayPolicy policy;
        policy.key_type = core::KeyType::SERVICE;
        policy.max_age_seconds = 30 * 24 * 60 * 60;
        policy.inactivity_threshold = 7 * 24 * 60 * 60;
        policy.requires_activity = true;
        policy.min_actions_per_epoch = 1;
        key_policies_[core::KeyType::SERVICE] = policy;
    }
    
    // ROUTING keys - requires routing performance
    {
        DecayPolicy policy;
        policy.key_type = core::KeyType::ROUTING;
        policy.max_age_seconds = 30 * 24 * 60 * 60;
        policy.inactivity_threshold = 7 * 24 * 60 * 60;
        policy.requires_activity = true;
        policy.min_actions_per_epoch = 10;
        policy.requires_performance = true;
        policy.min_success_rate = 0.7f;
        key_policies_[core::KeyType::ROUTING] = policy;
    }
    
    // NETWORK keys - 30 day expiry
    {
        DecayPolicy policy;
        policy.key_type = core::KeyType::NETWORK;
        policy.max_age_seconds = 30 * 24 * 60 * 60;
        policy.inactivity_threshold = 14 * 24 * 60 * 60;
        policy.requires_activity = false;
        key_policies_[core::KeyType::NETWORK] = policy;
    }
    
    CASHEW_LOG_DEBUG("Initialized {} default decay policies", key_policies_.size());
}

bool DecayScheduler::should_decay_key(const NodeID& node_id, core::KeyType key_type,
                                       const DecayPolicy& policy, DecayReason& reason) const
{
    auto activity_it = node_activities_.find(node_id);
    
    // No policy = no decay
    if (policy.max_age_seconds == 0) {
        return false;
    }
    
    // Check inactivity
    if (activity_it != node_activities_.end()) {
        if (activity_it->second.is_inactive(policy.inactivity_threshold)) {
            reason = DecayReason::INACTIVITY;
            return true;
        }
        
        // Check key-specific activity
        if (policy.requires_activity) {
            if (!activity_it->second.has_used_key_type(key_type, policy.inactivity_threshold)) {
                reason = DecayReason::INACTIVITY;
                return true;
            }
            
            // Check minimum actions
            auto epoch_actions = activity_it->second.actions_this_epoch.find(key_type);
            if (epoch_actions == activity_it->second.actions_this_epoch.end() ||
                epoch_actions->second < policy.min_actions_per_epoch) {
                reason = DecayReason::POOR_PERFORMANCE;
                return true;
            }
        }
    } else {
        // No activity tracked = assume inactive
        if (policy.requires_activity) {
            reason = DecayReason::INACTIVITY;
            return true;
        }
    }
    
    // Check age (if we had key issuance timestamps)
    // TODO: Track key issuance time in ledger
    
    return false;
}

bool DecayScheduler::should_decay_thing(const ContentHash& content_hash,
                                         const ThingDecayPolicy& policy,
                                         DecayReason& reason) const
{
    auto activity_it = thing_activities_.find(content_hash);
    
    // Check inactivity
    if (activity_it != thing_activities_.end()) {
        if (activity_it->second.is_inactive(policy.inactivity_threshold)) {
            reason = DecayReason::INACTIVITY;
            return true;
        }
        
        // Check redundancy
        if (!activity_it->second.meets_redundancy(policy.min_hosts_required)) {
            reason = DecayReason::RESOURCE_SHORTAGE;
            return true;
        }
        
        // Check age
        uint64_t current = current_timestamp();
        uint64_t age = current - activity_it->second.created_at;
        if (age > policy.max_age_seconds && activity_it->second.access_count == 0) {
            reason = DecayReason::EXPIRATION;
            return true;
        }
    } else {
        // No activity tracked - check state
        auto thing_state = state_manager_.get_thing_state(content_hash);
        if (!thing_state) {
            return false;
        }
        
        if (thing_state->host_count() < policy.min_hosts_required) {
            reason = DecayReason::RESOURCE_SHORTAGE;
            return true;
        }
    }
    
    return false;
}

uint64_t DecayScheduler::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

void DecayScheduler::update_node_activity_from_state(const NodeID& node_id) {
    auto& activity = node_activities_[node_id];
    activity.node_id = node_id;
    
    if (state_manager_.is_node_active(node_id)) {
        activity.last_seen = current_timestamp();
    }
}

void DecayScheduler::update_thing_activity_from_state(const ContentHash& content_hash) {
    auto& activity = thing_activities_[content_hash];
    activity.content_hash = content_hash;
    
    auto thing_state = state_manager_.get_thing_state(content_hash);
    if (thing_state) {
        activity.current_hosts = std::vector<NodeID>(thing_state->hosts.begin(), 
                                                      thing_state->hosts.end());
        if (activity.created_at == 0) {
            activity.created_at = thing_state->created_at;
            activity.last_accessed = thing_state->created_at;
        }
    }
}

} // namespace cashew::decay
