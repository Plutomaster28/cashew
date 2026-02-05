#include "core/postake/postake.hpp"
#include "crypto/blake3.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <cmath>

namespace cashew::postake {

// ContributionMetrics methods

float ContributionMetrics::uptime_percentage() const {
    if (first_seen == 0) {
        return 0.0f;
    }
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_time = static_cast<uint64_t>(now_time_t);
    
    uint64_t time_since_join = current_time - first_seen;
    if (time_since_join == 0) {
        return 100.0f;
    }
    
    return (static_cast<float>(total_uptime) / static_cast<float>(time_since_join)) * 100.0f;
}

float ContributionMetrics::routing_success_rate() const {
    uint32_t total_routes = successful_routes + failed_routes;
    if (total_routes == 0) {
        return 100.0f;
    }
    
    return (static_cast<float>(successful_routes) / static_cast<float>(total_routes)) * 100.0f;
}

// ContributionScore methods

void ContributionScore::calculate_total() {
    total_score = uptime_score + bandwidth_score + storage_score + routing_score + witness_score;
}

// ContributionTracker methods

void ContributionTracker::record_node_online(const NodeID& node_id) {
    online_status_[node_id] = true;
    online_since_[node_id] = current_timestamp();
    
    auto& metrics = metrics_[node_id];
    if (metrics.first_seen == 0) {
        metrics.first_seen = current_timestamp();
    }
    metrics.last_seen = current_timestamp();
}

void ContributionTracker::record_node_offline(const NodeID& node_id) {
    online_status_[node_id] = false;
    
    // Update uptime if was online
    auto online_it = online_since_.find(node_id);
    if (online_it != online_since_.end()) {
        uint64_t current = current_timestamp();
        uint64_t uptime = current - online_it->second;
        update_uptime(node_id, uptime);
        online_since_.erase(online_it);
    }
    
    metrics_[node_id].last_seen = current_timestamp();
}

void ContributionTracker::update_uptime(const NodeID& node_id, uint64_t seconds) {
    metrics_[node_id].total_uptime += seconds;
}

void ContributionTracker::record_bytes_routed(const NodeID& node_id, uint64_t bytes) {
    metrics_[node_id].bytes_routed += bytes;
}

void ContributionTracker::record_traffic(const NodeID& node_id, uint64_t sent, uint64_t received) {
    auto& metrics = metrics_[node_id];
    metrics.bytes_sent += sent;
    metrics.bytes_received += received;
}

void ContributionTracker::record_thing_hosted(const NodeID& node_id, uint64_t size_bytes) {
    auto& metrics = metrics_[node_id];
    metrics.things_hosted++;
    metrics.storage_bytes_provided += size_bytes;
}

void ContributionTracker::record_thing_removed(const NodeID& node_id, uint64_t size_bytes) {
    auto& metrics = metrics_[node_id];
    if (metrics.things_hosted > 0) {
        metrics.things_hosted--;
    }
    if (metrics.storage_bytes_provided >= size_bytes) {
        metrics.storage_bytes_provided -= size_bytes;
    }
}

void ContributionTracker::record_successful_route(const NodeID& node_id) {
    auto& metrics = metrics_[node_id];
    metrics.successful_routes++;
    update_routing_reliability(metrics);
}

void ContributionTracker::record_failed_route(const NodeID& node_id) {
    auto& metrics = metrics_[node_id];
    metrics.failed_routes++;
    update_routing_reliability(metrics);
}

void ContributionTracker::record_epoch_witness(const NodeID& node_id, uint64_t epoch) {
    (void)epoch;
    metrics_[node_id].epochs_witnessed++;
}

void ContributionTracker::record_epoch_missed(const NodeID& node_id, uint64_t epoch) {
    (void)epoch;
    metrics_[node_id].epochs_missed++;
}

ContributionMetrics ContributionTracker::get_metrics(const NodeID& node_id) const {
    auto it = metrics_.find(node_id);
    if (it == metrics_.end()) {
        return ContributionMetrics{};
    }
    
    // If node is currently online, add current session uptime
    auto metrics = it->second;
    auto online_it = online_since_.find(node_id);
    if (online_it != online_since_.end()) {
        uint64_t current = current_timestamp();
        uint64_t session_uptime = current - online_it->second;
        metrics.total_uptime += session_uptime;
    }
    
    return metrics;
}

std::vector<NodeID> ContributionTracker::get_active_contributors() const {
    std::vector<NodeID> result;
    
    uint64_t current = current_timestamp();
    static constexpr uint64_t ACTIVE_THRESHOLD = 300;  // 5 minutes
    
    for (const auto& [node_id, metrics] : metrics_) {
        if (current - metrics.last_seen <= ACTIVE_THRESHOLD) {
            result.push_back(node_id);
        }
    }
    
    return result;
}

void ContributionTracker::reset_metrics(const NodeID& node_id) {
    metrics_.erase(node_id);
    online_status_.erase(node_id);
    online_since_.erase(node_id);
}

void ContributionTracker::cleanup_inactive_nodes(uint64_t inactive_threshold) {
    std::vector<NodeID> to_remove;
    uint64_t current = current_timestamp();
    
    for (const auto& [node_id, metrics] : metrics_) {
        if (current - metrics.last_seen > inactive_threshold) {
            to_remove.push_back(node_id);
        }
    }
    
    for (const auto& node_id : to_remove) {
        reset_metrics(node_id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_INFO("Cleaned up {} inactive contributors", to_remove.size());
    }
}

uint64_t ContributionTracker::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

void ContributionTracker::update_routing_reliability(ContributionMetrics& metrics) {
    uint32_t total = metrics.successful_routes + metrics.failed_routes;
    if (total == 0) {
        metrics.routing_reliability = 1.0f;
        return;
    }
    
    metrics.routing_reliability = static_cast<float>(metrics.successful_routes) / static_cast<float>(total);
}

// PoStakeEngine methods

PoStakeEngine::PoStakeEngine(ledger::StateManager& state_manager)
    : state_manager_(state_manager)
{
    initialize_default_rates();
    CASHEW_LOG_INFO("PoStakeEngine initialized");
}

ContributionScore PoStakeEngine::calculate_score(const NodeID& node_id) const {
    auto metrics = tracker_.get_metrics(node_id);
    return calculate_score(metrics);
}

ContributionScore PoStakeEngine::calculate_score(const ContributionMetrics& metrics) const {
    ContributionScore score;
    score.node_id = metrics.node_id;
    
    score.uptime_score = calculate_uptime_score(metrics);
    score.bandwidth_score = calculate_bandwidth_score(metrics);
    score.storage_score = calculate_storage_score(metrics);
    score.routing_score = calculate_routing_score(metrics);
    score.witness_score = calculate_witness_score(metrics);
    
    score.calculate_total();
    
    return score;
}

void PoStakeEngine::set_earning_rate(core::KeyType key_type, const KeyEarningRate& rate) {
    earning_rates_[key_type] = rate;
}

KeyEarningRate PoStakeEngine::get_earning_rate(core::KeyType key_type) const {
    auto it = earning_rates_.find(key_type);
    if (it == earning_rates_.end()) {
        return KeyEarningRate{};
    }
    return it->second;
}

void PoStakeEngine::process_epoch(uint64_t epoch) {
    CASHEW_LOG_INFO("Processing PoStake epoch {}", epoch);
    
    auto rewards = calculate_epoch_rewards(epoch);
    
    // Award keys
    for (const auto& reward : rewards) {
        award_keys(reward);
    }
    
    epoch_rewards_[epoch] = rewards;
    
    CASHEW_LOG_INFO("PoStake epoch {} complete: {} rewards issued", epoch, rewards.size());
}

std::vector<PoStakeReward> PoStakeEngine::calculate_epoch_rewards(uint64_t epoch) const {
    std::vector<PoStakeReward> rewards;
    std::vector<EpochContribution> contributions;
    
    auto active_nodes = tracker_.get_active_contributors();
    
    for (const auto& node_id : active_nodes) {
        auto metrics = tracker_.get_metrics(node_id);
        auto score = calculate_score(metrics);
        
        // Determine key type and count
        auto key_type = determine_key_type(score);
        auto key_count = calculate_key_count(score, key_type);
        
        if (key_count > 0) {
            PoStakeReward reward;
            reward.node_id = node_id;
            reward.epoch = epoch;
            reward.key_type = key_type;
            reward.key_count = key_count;
            
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            reward.awarded_at = static_cast<uint64_t>(now_time_t);
            
            reward.proof_hash = hash_contribution(metrics);
            
            rewards.push_back(reward);
        }
    }
    
    return rewards;
}

bool PoStakeEngine::award_keys(const PoStakeReward& reward) {
    // TODO: Actually issue keys through ledger
    // For now, just log
    CASHEW_LOG_INFO("Awarded {} {} keys to node (epoch {})",
                   reward.key_count, static_cast<int>(reward.key_type), reward.epoch);
    return true;
}

std::optional<EpochContribution> PoStakeEngine::get_epoch_contribution(const NodeID& node_id, uint64_t epoch) const {
    auto it = epoch_contributions_.find(epoch);
    if (it == epoch_contributions_.end()) {
        return std::nullopt;
    }
    
    for (const auto& contribution : it->second) {
        if (contribution.node_id == node_id) {
            return contribution;
        }
    }
    
    return std::nullopt;
}

std::vector<EpochContribution> PoStakeEngine::get_node_history(const NodeID& node_id) const {
    std::vector<EpochContribution> result;
    
    for (const auto& [epoch, contributions] : epoch_contributions_) {
        for (const auto& contribution : contributions) {
            if (contribution.node_id == node_id) {
                result.push_back(contribution);
            }
        }
    }
    
    return result;
}

std::vector<NodeID> PoStakeEngine::get_top_contributors(ContributionType type, uint32_t count) const {
    auto active_nodes = tracker_.get_active_contributors();
    
    std::vector<std::pair<NodeID, uint64_t>> scored_nodes;
    
    for (const auto& node_id : active_nodes) {
        auto metrics = tracker_.get_metrics(node_id);
        uint64_t type_score = 0;
        
        switch (type) {
            case ContributionType::UPTIME:
                type_score = metrics.total_uptime;
                break;
            case ContributionType::BANDWIDTH:
                type_score = metrics.bytes_routed;
                break;
            case ContributionType::STORAGE:
                type_score = metrics.storage_bytes_provided;
                break;
            case ContributionType::ROUTING_QUALITY:
                type_score = metrics.successful_routes;
                break;
            case ContributionType::EPOCH_WITNESS:
                type_score = metrics.epochs_witnessed;
                break;
        }
        
        scored_nodes.push_back({node_id, type_score});
    }
    
    std::sort(scored_nodes.begin(), scored_nodes.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<NodeID> result;
    for (size_t i = 0; i < std::min(static_cast<size_t>(count), scored_nodes.size()); i++) {
        result.push_back(scored_nodes[i].first);
    }
    
    return result;
}

uint32_t PoStakeEngine::get_total_keys_awarded(uint64_t epoch) const {
    auto it = epoch_rewards_.find(epoch);
    if (it == epoch_rewards_.end()) {
        return 0;
    }
    
    uint32_t total = 0;
    for (const auto& reward : it->second) {
        total += reward.key_count;
    }
    
    return total;
}

uint32_t PoStakeEngine::get_average_contribution_score() const {
    auto active_nodes = tracker_.get_active_contributors();
    if (active_nodes.empty()) {
        return 0;
    }
    
    uint64_t total_score = 0;
    for (const auto& node_id : active_nodes) {
        auto score = calculate_score(node_id);
        total_score += score.total_score;
    }
    
    return static_cast<uint32_t>(total_score / active_nodes.size());
}

void PoStakeEngine::initialize_default_rates() {
    // SERVICE keys (for hosting Things)
    {
        KeyEarningRate rate;
        rate.key_type = core::KeyType::SERVICE;
        rate.points_per_key = 500;
        rate.max_per_epoch = 5;
        rate.min_score_required = 200;
        earning_rates_[core::KeyType::SERVICE] = rate;
    }
    
    // ROUTING keys (for traffic routing)
    {
        KeyEarningRate rate;
        rate.key_type = core::KeyType::ROUTING;
        rate.points_per_key = 300;
        rate.max_per_epoch = 10;
        rate.min_score_required = 100;
        earning_rates_[core::KeyType::ROUTING] = rate;
    }
    
    // NETWORK keys (for joining networks)
    {
        KeyEarningRate rate;
        rate.key_type = core::KeyType::NETWORK;
        rate.points_per_key = 400;
        rate.max_per_epoch = 3;
        rate.min_score_required = 150;
        earning_rates_[core::KeyType::NETWORK] = rate;
    }
}

uint32_t PoStakeEngine::calculate_uptime_score(const ContributionMetrics& metrics) const {
    // Score based on uptime percentage and absolute time
    float uptime_pct = metrics.uptime_percentage();
    
    // Base score from percentage (0-100)
    uint32_t base_score = static_cast<uint32_t>(uptime_pct);
    
    // Bonus for long uptime (up to 100 points)
    static constexpr uint64_t MONTH_SECONDS = 30 * 24 * 60 * 60;
    uint32_t longevity_bonus = std::min(100u, static_cast<uint32_t>((metrics.total_uptime * 100) / MONTH_SECONDS));
    
    return static_cast<uint32_t>((base_score + longevity_bonus) * UPTIME_WEIGHT);
}

uint32_t PoStakeEngine::calculate_bandwidth_score(const ContributionMetrics& metrics) const {
    // Score based on bytes routed
    static constexpr uint64_t GB = 1024 * 1024 * 1024;
    
    // 1 point per GB routed
    uint32_t score = static_cast<uint32_t>(metrics.bytes_routed / GB);
    
    // Cap at 200 points
    if (score > 200) score = 200;
    
    return static_cast<uint32_t>(score * BANDWIDTH_WEIGHT);
}

uint32_t PoStakeEngine::calculate_storage_score(const ContributionMetrics& metrics) const {
    // Score based on Things hosted and storage provided
    static constexpr uint64_t GB = 1024 * 1024 * 1024;
    
    // 10 points per Thing hosted
    uint32_t thing_score = metrics.things_hosted * 10;
    
    // 1 point per GB stored
    uint32_t storage_score = static_cast<uint32_t>(metrics.storage_bytes_provided / GB);
    
    uint32_t total = thing_score + storage_score;
    if (total > 200) total = 200;
    
    return static_cast<uint32_t>(total * STORAGE_WEIGHT);
}

uint32_t PoStakeEngine::calculate_routing_score(const ContributionMetrics& metrics) const {
    // Score based on successful routes and reliability
    uint32_t base_score = std::min(100u, metrics.successful_routes);
    
    // Multiply by reliability (0.0 to 1.0)
    uint32_t reliability_adjusted = static_cast<uint32_t>(base_score * metrics.routing_reliability);
    
    return static_cast<uint32_t>(reliability_adjusted * ROUTING_WEIGHT);
}

uint32_t PoStakeEngine::calculate_witness_score(const ContributionMetrics& metrics) const {
    // Score based on epoch witnessing
    uint32_t total_epochs = metrics.epochs_witnessed + metrics.epochs_missed;
    if (total_epochs == 0) {
        return 0;
    }
    
    float participation_rate = static_cast<float>(metrics.epochs_witnessed) / static_cast<float>(total_epochs);
    uint32_t score = static_cast<uint32_t>(participation_rate * 100.0f);
    
    return static_cast<uint32_t>(score * WITNESS_WEIGHT);
}

core::KeyType PoStakeEngine::determine_key_type(const ContributionScore& score) const {
    // Determine which key type to award based on contribution profile
    if (score.storage_score > score.bandwidth_score && score.storage_score > score.routing_score) {
        return core::KeyType::SERVICE;  // Good at hosting
    } else if (score.bandwidth_score > score.storage_score && score.bandwidth_score > score.routing_score) {
        return core::KeyType::ROUTING;  // Good at routing
    } else {
        return core::KeyType::NETWORK;  // Balanced contribution
    }
}

uint32_t PoStakeEngine::calculate_key_count(const ContributionScore& score, core::KeyType key_type) const {
    auto rate = get_earning_rate(key_type);
    
    if (score.total_score < rate.min_score_required) {
        return 0;
    }
    
    uint32_t keys = score.total_score / rate.points_per_key;
    
    if (keys > rate.max_per_epoch) {
        keys = rate.max_per_epoch;
    }
    
    return keys;
}

Hash256 PoStakeEngine::hash_contribution(const ContributionMetrics& metrics) const {
    // Serialize metrics into bytes for hashing
    std::vector<uint8_t> data;
    
    auto append_uint64 = [&data](uint64_t value) {
        for (int i = 0; i < 8; i++) {
            data.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    };
    
    auto append_uint32 = [&data](uint32_t value) {
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    };
    
    append_uint64(metrics.total_uptime);
    append_uint64(metrics.bytes_routed);
    append_uint64(metrics.storage_bytes_provided);
    append_uint32(metrics.successful_routes);
    append_uint32(metrics.epochs_witnessed);
    
    return crypto::Blake3::hash(data);
}

} // namespace cashew::postake
