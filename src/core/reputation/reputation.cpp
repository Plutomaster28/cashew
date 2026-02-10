#include "core/reputation/reputation.hpp"
#include "core/reputation/attestation.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <queue>
#include <cmath>

namespace cashew::reputation {

// Attestation methods

bool Attestation::is_expired() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t) >= expires_at;
}

bool Attestation::is_vouch() const {
    return score_delta > 0;
}

bool Attestation::is_denouncement() const {
    return score_delta < 0;
}

// VouchRecord methods

int32_t VouchRecord::calculate_voucher_impact() const {
    if (!still_active) {
        return 0;
    }
    
    // Voucher's reputation changes based on vouchee's behavior
    int32_t reputation_change = vouchee_current_reputation - vouchee_reputation_at_vouch;
    
    // Voucher gets 50% of vouchee's reputation change
    int32_t impact = reputation_change / 2;
    
    // Cap the impact
    if (impact > 50) impact = 50;
    if (impact < -50) impact = -50;
    
    return impact;
}

// ReputationScore methods

float ReputationScore::trust_level() const {
    // Convert score to 0.0-1.0 range
    float normalized = static_cast<float>(total_score + 1000) / 11000.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    return normalized;
}

// TrustGraph methods

void TrustGraph::add_edge(const NodeID& from, const NodeID& to, float weight) {
    if (weight < 0.0f) weight = 0.0f;
    if (weight > 1.0f) weight = 1.0f;
    
    TrustEdge edge;
    edge.from = from;
    edge.to = to;
    edge.trust_weight = weight;
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    edge.established_at = static_cast<uint64_t>(now_time_t);
    edge.last_updated = edge.established_at;
    
    edges_[from][to] = edge;
}

void TrustGraph::remove_edge(const NodeID& from, const NodeID& to) {
    auto from_it = edges_.find(from);
    if (from_it != edges_.end()) {
        from_it->second.erase(to);
    }
}

void TrustGraph::update_edge_weight(const NodeID& from, const NodeID& to, float weight) {
    auto from_it = edges_.find(from);
    if (from_it == edges_.end()) {
        add_edge(from, to, weight);
        return;
    }
    
    auto to_it = from_it->second.find(to);
    if (to_it == from_it->second.end()) {
        add_edge(from, to, weight);
        return;
    }
    
    if (weight < 0.0f) weight = 0.0f;
    if (weight > 1.0f) weight = 1.0f;
    
    to_it->second.trust_weight = weight;
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    to_it->second.last_updated = static_cast<uint64_t>(now_time_t);
}

std::optional<float> TrustGraph::get_direct_trust(const NodeID& from, const NodeID& to) const {
    auto from_it = edges_.find(from);
    if (from_it == edges_.end()) {
        return std::nullopt;
    }
    
    auto to_it = from_it->second.find(to);
    if (to_it == from_it->second.end()) {
        return std::nullopt;
    }
    
    return to_it->second.trust_weight;
}

float TrustGraph::calculate_transitive_trust(const NodeID& from, const NodeID& to, uint32_t max_hops) const {
    // BFS to find shortest path
    if (from == to) {
        return 1.0f;
    }
    
    // Check direct trust first
    auto direct = get_direct_trust(from, to);
    if (direct) {
        return *direct;
    }
    
    // BFS for path finding
    std::map<NodeID, float> best_trust;
    std::queue<std::pair<NodeID, uint32_t>> queue;
    
    queue.push({from, 0});
    best_trust[from] = 1.0f;
    
    while (!queue.empty()) {
        auto [current, hops] = queue.front();
        queue.pop();
        
        if (hops >= max_hops) {
            continue;
        }
        
        auto current_edges = edges_.find(current);
        if (current_edges == edges_.end()) {
            continue;
        }
        
        for (const auto& [next_node, edge] : current_edges->second) {
            float path_trust = best_trust[current] * edge.trust_weight;
            
            auto existing = best_trust.find(next_node);
            if (existing == best_trust.end() || path_trust > existing->second) {
                best_trust[next_node] = path_trust;
                queue.push({next_node, hops + 1});
            }
        }
    }
    
    auto result = best_trust.find(to);
    if (result == best_trust.end()) {
        return 0.0f;
    }
    
    return result->second;
}

std::vector<NodeID> TrustGraph::get_trusted_by(const NodeID& node) const {
    std::vector<NodeID> result;
    
    for (const auto& [from_node, edges] : edges_) {
        auto it = edges.find(node);
        if (it != edges.end() && it->second.trust_weight > 0.3f) {
            result.push_back(from_node);
        }
    }
    
    return result;
}

std::vector<NodeID> TrustGraph::get_trusts(const NodeID& node) const {
    std::vector<NodeID> result;
    
    auto it = edges_.find(node);
    if (it == edges_.end()) {
        return result;
    }
    
    for (const auto& [to_node, edge] : it->second) {
        if (edge.trust_weight > 0.3f) {
            result.push_back(to_node);
        }
    }
    
    return result;
}

std::set<NodeID> TrustGraph::find_trust_community(const NodeID& node, float min_trust) const {
    std::set<NodeID> community;
    std::queue<NodeID> to_explore;
    
    community.insert(node);
    to_explore.push(node);
    
    while (!to_explore.empty()) {
        NodeID current = to_explore.front();
        to_explore.pop();
        
        auto it = edges_.find(current);
        if (it == edges_.end()) {
            continue;
        }
        
        for (const auto& [neighbor, edge] : it->second) {
            if (edge.trust_weight >= min_trust && community.find(neighbor) == community.end()) {
                community.insert(neighbor);
                to_explore.push(neighbor);
            }
        }
    }
    
    return community;
}

void TrustGraph::decay_edge_weights(float decay_factor) {
    for (auto& [from_node, edges] : edges_) {
        for (auto& [to_node, edge] : edges) {
            edge.trust_weight *= decay_factor;
        }
    }
}

void TrustGraph::prune_weak_edges(float threshold) {
    std::vector<std::pair<NodeID, NodeID>> to_remove;
    
    for (const auto& [from_node, edges] : edges_) {
        for (const auto& [to_node, edge] : edges) {
            if (edge.trust_weight < threshold) {
                to_remove.push_back({from_node, to_node});
            }
        }
    }
    
    for (const auto& [from, to] : to_remove) {
        remove_edge(from, to);
    }
}

float TrustGraph::calculate_path_trust(const std::vector<NodeID>& path) const {
    if (path.size() < 2) {
        return 1.0f;
    }
    
    float trust = 1.0f;
    for (size_t i = 0; i < path.size() - 1; i++) {
        auto edge_trust = get_direct_trust(path[i], path[i + 1]);
        if (!edge_trust) {
            return 0.0f;
        }
        trust *= *edge_trust;
    }
    
    return trust;
}

// ReputationManager methods

ReputationManager::ReputationManager(ledger::StateManager& state_manager)
    : state_manager_(state_manager)
{
    CASHEW_LOG_INFO("ReputationManager initialized");
}

int32_t ReputationManager::get_reputation(const NodeID& node_id) const {
    auto it = scores_.find(node_id);
    if (it == scores_.end()) {
        return 0;
    }
    return it->second.total_score;
}

ReputationScore ReputationManager::get_detailed_score(const NodeID& node_id) const {
    auto it = scores_.find(node_id);
    if (it == scores_.end()) {
        ReputationScore score;
        score.node_id = node_id;
        score.total_score = 0;
        score.hosting_score = 0;
        score.contribution_score = 0;
        score.vouching_score = 0;
        score.penalty_score = 0;
        score.things_hosted = 0;
        score.bandwidth_contributed = 0;
        score.successful_vouches = 0;
        score.failed_vouches = 0;
        score.violations = 0;
        return score;
    }
    return it->second;
}

void ReputationManager::record_action(const NodeID& node_id, ReputationAction action,
                                       const std::optional<NodeID>& related_node)
{
    initialize_score(node_id);
    
    int32_t delta = get_action_score(action);
    auto& score = scores_[node_id];
    
    // Update component scores
    switch (action) {
        case ReputationAction::HOST_THING:
            score.hosting_score += delta;
            score.things_hosted++;
            break;
        case ReputationAction::CONTRIBUTE_BANDWIDTH:
            score.contribution_score += delta;
            score.bandwidth_contributed++;
            break;
        case ReputationAction::SUBMIT_POW:
        case ReputationAction::PROVIDE_POSTAKE:
            score.contribution_score += delta;
            break;
        case ReputationAction::VOUCH_SUCCESSFUL:
            score.vouching_score += delta;
            score.successful_vouches++;
            break;
        case ReputationAction::VOUCH_FAILED:
            score.penalty_score += delta;
            score.failed_vouches++;
            break;
        case ReputationAction::OFFLINE_PROLONGED:
        case ReputationAction::NETWORK_VIOLATION:
        case ReputationAction::SPAM_DETECTED:
        case ReputationAction::CONTENT_REMOVED:
            score.penalty_score += delta;
            score.violations++;
            break;
    }
    
    score.total_score += delta;
    clamp_reputation(score.total_score);
    
    // Record event
    ReputationEvent event;
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    event.timestamp = static_cast<uint64_t>(now_time_t);
    event.action = action;
    event.score_delta = delta;
    event.score_after = score.total_score;
    event.related_node = related_node;
    
    score.recent_events.push_back(event);
    
    // Keep only last 100 events
    if (score.recent_events.size() > 100) {
        score.recent_events.erase(score.recent_events.begin());
    }
    
    CASHEW_LOG_DEBUG("Reputation action recorded: {} (delta: {}, new score: {})",
                    static_cast<int>(action), delta, score.total_score);
}

void ReputationManager::apply_score_delta(const NodeID& node_id, int32_t delta, const std::string& reason) {
    initialize_score(node_id);
    
    auto& score = scores_[node_id];
    score.total_score += delta;
    clamp_reputation(score.total_score);
    
    ReputationEvent event;
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    event.timestamp = static_cast<uint64_t>(now_time_t);
    event.score_delta = delta;
    event.score_after = score.total_score;
    event.details = reason;
    
    score.recent_events.push_back(event);
    if (score.recent_events.size() > 100) {
        score.recent_events.erase(score.recent_events.begin());
    }
    
    CASHEW_LOG_DEBUG("Reputation delta applied: {} (reason: {}, new score: {})",
                    delta, reason, score.total_score);
}

bool ReputationManager::create_attestation(const Attestation& attestation) {
    if (attestation.is_expired()) {
        CASHEW_LOG_WARN("Attempted to create expired attestation");
        return false;
    }
    
    // TODO: Verify signature
    
    // Store attestation
    attestations_[attestation.subject].push_back(attestation);
    
    // Apply reputation delta
    apply_score_delta(attestation.subject, attestation.score_delta, attestation.statement);
    
    // Update trust graph
    update_trust_from_attestation(attestation);
    
    CASHEW_LOG_INFO("Attestation created");
    return true;
}

bool ReputationManager::verify_attestation(const Attestation& attestation) const {
    if (attestation.is_expired()) {
        return false;
    }
    
    // Basic validation
    if (attestation.attester.id == NodeID().id) {
        return false;  // Invalid attester
    }
    if (attestation.subject.id == NodeID().id) {
        return false;  // Invalid subject
    }
    
    // TODO: In production, retrieve attester's public key from identity system
    // For now, we perform basic validation. When a public key registry/identity
    // system is available, the signature can be cryptographically verified:
    //
    // auto public_key = identity_system.get_public_key(attestation.attester);
    // if (!public_key) {
    //     CASHEW_LOG_WARN("Cannot verify attestation: attester public key not found");
    //     return false;
    // }
    // return AttestationSigner::verify_attestation_signature(attestation, *public_key);
    
    return true;  // Accept for now (structural validation passed)
}

std::vector<Attestation> ReputationManager::get_attestations_for(const NodeID& subject) const {
    auto it = attestations_.find(subject);
    if (it == attestations_.end()) {
        return {};
    }
    return it->second;
}

std::vector<Attestation> ReputationManager::get_attestations_by(const NodeID& attester) const {
    std::vector<Attestation> result;
    
    for (const auto& [subject, attestations] : attestations_) {
        for (const auto& attestation : attestations) {
            if (attestation.attester == attester) {
                result.push_back(attestation);
            }
        }
    }
    
    return result;
}

bool ReputationManager::vouch_for_node(const NodeID& voucher, const NodeID& vouchee) {
    if (!can_vouch(voucher, vouchee)) {
        return false;
    }
    
    VouchRecord record;
    record.voucher = voucher;
    record.vouchee = vouchee;
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    record.vouched_at = static_cast<uint64_t>(now_time_t);
    record.still_active = true;
    record.vouchee_reputation_at_vouch = get_reputation(vouchee);
    record.vouchee_current_reputation = record.vouchee_reputation_at_vouch;
    
    vouches_[voucher].push_back(record);
    
    // Add trust edge
    trust_graph_.add_edge(voucher, vouchee, 0.8f);
    
    CASHEW_LOG_INFO("Vouch created");
    return true;
}

bool ReputationManager::can_vouch(const NodeID& voucher, const NodeID& vouchee) const {
    // Voucher needs sufficient reputation
    int32_t voucher_rep = get_reputation(voucher);
    if (voucher_rep < VOUCH_REPUTATION_REQUIREMENT) {
        return false;
    }
    
    // Vouchee must be active
    if (!state_manager_.is_node_active(vouchee)) {
        return false;
    }
    
    // Check vouch limit
    auto it = vouches_.find(voucher);
    if (it != vouches_.end()) {
        size_t active_vouches = 0;
        for (const auto& vouch : it->second) {
            if (vouch.still_active) {
                active_vouches++;
            }
        }
        if (active_vouches >= MAX_VOUCHES_PER_NODE) {
            return false;
        }
    }
    
    return true;
}

std::vector<VouchRecord> ReputationManager::get_vouches_by(const NodeID& voucher) const {
    auto it = vouches_.find(voucher);
    if (it == vouches_.end()) {
        return {};
    }
    return it->second;
}

std::vector<VouchRecord> ReputationManager::get_vouches_for(const NodeID& vouchee) const {
    std::vector<VouchRecord> result;
    
    for (const auto& [voucher, vouches] : vouches_) {
        for (const auto& vouch : vouches) {
            if (vouch.vouchee == vouchee) {
                result.push_back(vouch);
            }
        }
    }
    
    return result;
}

void ReputationManager::update_vouch_impacts() {
    for (auto& [voucher, vouches] : vouches_) {
        for (auto& vouch : vouches) {
            if (!vouch.still_active) {
                continue;
            }
            
            // Update vouchee's current reputation
            vouch.vouchee_current_reputation = get_reputation(vouch.vouchee);
            
            // Calculate impact on voucher
            int32_t impact = vouch.calculate_voucher_impact();
            if (impact != 0) {
                apply_score_delta(voucher, impact, "Vouch impact");
            }
        }
    }
}

void ReputationManager::rebuild_trust_graph() {
    trust_graph_ = TrustGraph();
    
    // Add edges from attestations
    for (const auto& [subject, attestations] : attestations_) {
        for (const auto& attestation : attestations) {
            if (!attestation.is_expired()) {
                update_trust_from_attestation(attestation);
            }
        }
    }
    
    // Add edges from vouches
    for (const auto& [voucher, vouches] : vouches_) {
        for (const auto& vouch : vouches) {
            if (vouch.still_active) {
                trust_graph_.add_edge(voucher, vouch.vouchee, 0.8f);
            }
        }
    }
    
    CASHEW_LOG_INFO("Trust graph rebuilt");
}

std::vector<NodeID> ReputationManager::get_top_reputation(uint32_t count) const {
    std::vector<std::pair<NodeID, int32_t>> scored_nodes;
    
    for (const auto& [node_id, score] : scores_) {
        scored_nodes.push_back({node_id, score.total_score});
    }
    
    std::sort(scored_nodes.begin(), scored_nodes.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<NodeID> result;
    for (size_t i = 0; i < std::min(static_cast<size_t>(count), scored_nodes.size()); i++) {
        result.push_back(scored_nodes[i].first);
    }
    
    return result;
}

std::vector<NodeID> ReputationManager::get_suspicious_nodes() const {
    std::vector<NodeID> result;
    
    for (const auto& [node_id, score] : scores_) {
        if (score.is_suspicious()) {
            result.push_back(node_id);
        }
    }
    
    return result;
}

int32_t ReputationManager::get_average_reputation() const {
    if (scores_.empty()) {
        return 0;
    }
    
    int64_t sum = 0;
    for (const auto& [node_id, score] : scores_) {
        sum += score.total_score;
    }
    
    return static_cast<int32_t>(sum / scores_.size());
}

int32_t ReputationManager::get_median_reputation() const {
    if (scores_.empty()) {
        return 0;
    }
    
    std::vector<int32_t> all_scores;
    all_scores.reserve(scores_.size());
    for (const auto& [node_id, score] : scores_) {
        all_scores.push_back(score.total_score);
    }
    
    std::sort(all_scores.begin(), all_scores.end());
    return all_scores[all_scores.size() / 2];
}

size_t ReputationManager::count_trustworthy_nodes() const {
    size_t count = 0;
    for (const auto& [node_id, score] : scores_) {
        if (score.is_trustworthy()) {
            count++;
        }
    }
    return count;
}

void ReputationManager::decay_reputation() {
    for (auto& [node_id, score] : scores_) {
        // Decay toward zero
        score.total_score = static_cast<int32_t>(score.total_score * REPUTATION_DECAY_RATE);
        score.hosting_score = static_cast<int32_t>(score.hosting_score * REPUTATION_DECAY_RATE);
        score.contribution_score = static_cast<int32_t>(score.contribution_score * REPUTATION_DECAY_RATE);
        score.vouching_score = static_cast<int32_t>(score.vouching_score * REPUTATION_DECAY_RATE);
        score.penalty_score = static_cast<int32_t>(score.penalty_score * REPUTATION_DECAY_RATE);
    }
    
    // Decay trust graph edges
    trust_graph_.decay_edge_weights(0.95f);
}

void ReputationManager::cleanup_expired_attestations() {
    size_t removed = 0;
    
    for (auto& [subject, attestations] : attestations_) {
        auto original_size = attestations.size();
        attestations.erase(
            std::remove_if(attestations.begin(), attestations.end(),
                          [](const Attestation& a) { return a.is_expired(); }),
            attestations.end()
        );
        removed += original_size - attestations.size();
    }
    
    if (removed > 0) {
        CASHEW_LOG_INFO("Cleaned up {} expired attestations", removed);
    }
}

void ReputationManager::initialize_score(const NodeID& node_id) {
    if (scores_.find(node_id) == scores_.end()) {
        ReputationScore score;
        score.node_id = node_id;
        score.total_score = 0;
        score.hosting_score = 0;
        score.contribution_score = 0;
        score.vouching_score = 0;
        score.penalty_score = 0;
        score.things_hosted = 0;
        score.bandwidth_contributed = 0;
        score.successful_vouches = 0;
        score.failed_vouches = 0;
        score.violations = 0;
        scores_[node_id] = score;
    }
}

int32_t ReputationManager::get_action_score(ReputationAction action) const {
    switch (action) {
        case ReputationAction::HOST_THING:
            return 10;
        case ReputationAction::CONTRIBUTE_BANDWIDTH:
            return 5;
        case ReputationAction::SUBMIT_POW:
            return 2;
        case ReputationAction::PROVIDE_POSTAKE:
            return 15;
        case ReputationAction::VOUCH_SUCCESSFUL:
            return 20;
        case ReputationAction::OFFLINE_PROLONGED:
            return -10;
        case ReputationAction::NETWORK_VIOLATION:
            return -50;
        case ReputationAction::SPAM_DETECTED:
            return -30;
        case ReputationAction::VOUCH_FAILED:
            return -40;
        case ReputationAction::CONTENT_REMOVED:
            return -20;
        default:
            return 0;
    }
}

void ReputationManager::update_trust_from_attestation(const Attestation& attestation) {
    // Convert attestation score to trust weight
    float trust_weight = 0.5f;  // Neutral
    
    if (attestation.score_delta > 50) {
        trust_weight = 0.9f;  // Strong positive
    } else if (attestation.score_delta > 20) {
        trust_weight = 0.7f;  // Moderate positive
    } else if (attestation.score_delta > 0) {
        trust_weight = 0.6f;  // Weak positive
    } else if (attestation.score_delta < -50) {
        trust_weight = 0.1f;  // Strong negative
    } else if (attestation.score_delta < -20) {
        trust_weight = 0.3f;  // Moderate negative
    } else if (attestation.score_delta < 0) {
        trust_weight = 0.4f;  // Weak negative
    }
    
    trust_graph_.add_edge(attestation.attester, attestation.subject, trust_weight);
}

void ReputationManager::clamp_reputation(int32_t& score) const {
    if (score < REPUTATION_FLOOR) {
        score = REPUTATION_FLOOR;
    }
    if (score > REPUTATION_CEILING) {
        score = REPUTATION_CEILING;
    }
}

} // namespace cashew::reputation
