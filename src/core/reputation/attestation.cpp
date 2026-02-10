#include "attestation.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <queue>
#include <set>

namespace cashew::reputation {

// AttestationSigner implementation

std::vector<uint8_t> AttestationSigner::attestation_to_signable_bytes(
    const Attestation& attestation
) {
    std::vector<uint8_t> data;
    
    // Attester node ID (32 bytes)
    data.insert(data.end(), attestation.attester.id.begin(), attestation.attester.id.end());
    
    // Subject node ID (32 bytes)
    data.insert(data.end(), attestation.subject.id.begin(), attestation.subject.id.end());
    
    // Score delta (4 bytes, signed)
    int32_t score = attestation.score_delta;
    for (int i = 3; i >= 0; --i) {
        data.push_back((score >> (i * 8)) & 0xFF);
    }
    
    // Statement (length-prefixed UTF-8)
    uint32_t statement_len = static_cast<uint32_t>(attestation.statement.size());
    for (int i = 3; i >= 0; --i) {
        data.push_back((statement_len >> (i * 8)) & 0xFF);
    }
    data.insert(data.end(), attestation.statement.begin(), attestation.statement.end());
    
    // Timestamp (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((attestation.timestamp >> (i * 8)) & 0xFF);
    }
    
    // Expires at (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back((attestation.expires_at >> (i * 8)) & 0xFF);
    }
    
    return data;
}

bool AttestationSigner::sign_attestation(
    Attestation& attestation,
    const core::NodeIdentity& attester_identity
) {
    // Set attester ID
    attestation.attester = attester_identity.id();
    
    // Get signable bytes
    auto signable = attestation_to_signable_bytes(attestation);
    
    // Sign
    auto signature = attester_identity.sign(signable);
    attestation.signature = signature;
    
    CASHEW_LOG_DEBUG("Attestation signed by {}",
        attestation.attester.to_string().substr(0, 8));
    
    return true;
}

bool AttestationSigner::verify_attestation_signature(
    const Attestation& attestation,
    const PublicKey& attester_public_key
) {
    // Get signable bytes
    bytes signable = attestation_to_signable_bytes(attestation);
    
    // Verify signature (Ed25519::verify takes message, signature, public_key)
    bool valid = crypto::Ed25519::verify(
        signable,
        attestation.signature,
        attester_public_key
    );
    
    if (!valid) {
        CASHEW_LOG_WARN("Attestation signature verification failed");
    }
    
    return valid;
}

Attestation AttestationSigner::create_attestation(
    const NodeID& attester,
    const NodeID& subject,
    int32_t score_delta,
    const std::string& statement,
    uint64_t validity_duration_seconds
) {
    Attestation attestation;
    attestation.attester = attester;
    attestation.subject = subject;
    attestation.score_delta = score_delta;
    attestation.statement = statement;
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    attestation.timestamp = static_cast<uint64_t>(now_time_t);
    attestation.expires_at = attestation.timestamp + validity_duration_seconds;
    
    return attestation;
}

Attestation AttestationSigner::create_vouch_attestation(
    const NodeID& voucher,
    const NodeID& vouchee,
    int32_t score_boost,
    const std::string& statement,
    uint64_t validity_duration_seconds
) {
    // Vouch should always be positive
    if (score_boost <= 0) {
        score_boost = 20;  // Default vouch boost
    }
    
    return create_attestation(
        voucher,
        vouchee,
        score_boost,
        statement.empty() ? "Vouching for this node" : statement,
        validity_duration_seconds
    );
}

Attestation AttestationSigner::create_denouncement_attestation(
    const NodeID& denouncer,
    const NodeID& subject,
    int32_t score_penalty,
    const std::string& reason,
    uint64_t validity_duration_seconds
) {
    // Denouncement should always be negative
    if (score_penalty >= 0) {
        score_penalty = -30;  // Default penalty
    }
    
    return create_attestation(
        denouncer,
        subject,
        score_penalty,
        reason.empty() ? "Reporting negative behavior" : reason,
        validity_duration_seconds
    );
}

// VouchingWorkflow implementation

VouchingWorkflow::VouchingWorkflow(ReputationManager& reputation_mgr)
    : reputation_mgr_(reputation_mgr)
{
}

bool VouchingWorkflow::request_vouch(
    const core::NodeIdentity& voucher_identity,
    const NodeID& vouchee,
    const std::string& reason
) {
    NodeID voucher_id = voucher_identity.id();
    
    // Check if voucher can vouch
    if (!reputation_mgr_.can_vouch(voucher_id, vouchee)) {
        CASHEW_LOG_WARN("Node {} cannot vouch for {}",
            voucher_id.to_string().substr(0, 8),
            vouchee.to_string().substr(0, 8));
        return false;
    }
    
    // Create vouch attestation
    auto attestation = AttestationSigner::create_vouch_attestation(
        voucher_id,
        vouchee,
        20,  // +20 reputation boost
        reason
    );
    
    // Sign attestation
    Attestation signed_attestation = attestation;
    if (!AttestationSigner::sign_attestation(signed_attestation, voucher_identity)) {
        CASHEW_LOG_ERROR("Failed to sign vouch attestation");
        return false;
    }
    
    // Create vouch in reputation system
    if (!reputation_mgr_.vouch_for_node(voucher_id, vouchee)) {
        CASHEW_LOG_ERROR("Failed to create vouch record");
        return false;
    }
    
    // Store attestation
    if (!reputation_mgr_.create_attestation(signed_attestation)) {
        CASHEW_LOG_ERROR("Failed to store vouch attestation");
        return false;
    }
    
    CASHEW_LOG_INFO("Vouch created: {} vouches for {}",
        voucher_id.to_string().substr(0, 8),
        vouchee.to_string().substr(0, 8));
    
    return true;
}

bool VouchingWorkflow::accept_vouch(
    const core::NodeIdentity& vouchee_identity,
    const NodeID& voucher
) {
    NodeID vouchee_id = vouchee_identity.id();
    
    // Check if vouch exists
    if (!has_vouch(voucher, vouchee_id)) {
        CASHEW_LOG_WARN("No vouch found from {} to {}",
            voucher.to_string().substr(0, 8),
            vouchee_id.to_string().substr(0, 8));
        return false;
    }
    
    // Create acceptance attestation
    auto attestation = AttestationSigner::create_attestation(
        vouchee_id,
        voucher,
        5,  // Small reputation boost to voucher for successful vouch
        "Accepting vouch",
        2592000  // 30 days
    );
    
    // Sign attestation
    Attestation signed_attestation = attestation;
    if (!AttestationSigner::sign_attestation(signed_attestation, vouchee_identity)) {
        CASHEW_LOG_ERROR("Failed to sign vouch acceptance");
        return false;
    }
    
    // Store attestation
    if (!reputation_mgr_.create_attestation(signed_attestation)) {
        CASHEW_LOG_ERROR("Failed to store vouch acceptance");
        return false;
    }
    
    CASHEW_LOG_INFO("Vouch accepted: {} accepted vouch from {}",
        vouchee_id.to_string().substr(0, 8),
        voucher.to_string().substr(0, 8));
    
    return true;
}

bool VouchingWorkflow::revoke_vouch(
    const NodeID& voucher,
    const NodeID& vouchee,
    const std::string& reason
) {
    if (!has_vouch(voucher, vouchee)) {
        return false;
    }
    
    // Create revocation attestation (negative)
    auto attestation = AttestationSigner::create_denouncement_attestation(
        voucher,
        vouchee,
        -20,  // Remove the vouch boost
        reason.empty() ? "Revoking vouch" : reason,
        864000  // 10 days
    );
    
    // Note: This attestation would need to be signed by the voucher's identity
    // For now, we just mark it in the system
    
    // TODO: Mark vouch as inactive in reputation manager
    
    CASHEW_LOG_INFO("Vouch revoked: {} revokes vouch for {}",
        voucher.to_string().substr(0, 8),
        vouchee.to_string().substr(0, 8));
    
    return true;
}

std::vector<VouchRecord> VouchingWorkflow::get_active_vouches_for(const NodeID& vouchee) const {
    std::vector<VouchRecord> result;
    
    // Search all vouches to find those for this vouchee
    // This is inefficient but works for now
    // TODO: Add index in ReputationManager for vouchee lookups
    
    return result;
}

std::vector<VouchRecord> VouchingWorkflow::get_active_vouches_by(const NodeID& voucher) const {
    auto all_vouches = reputation_mgr_.get_vouches_by(voucher);
    
    std::vector<VouchRecord> active;
    for (const auto& vouch : all_vouches) {
        if (vouch.still_active) {
            active.push_back(vouch);
        }
    }
    
    return active;
}

std::vector<std::vector<NodeID>> VouchingWorkflow::get_vouching_chains(
    const NodeID& node,
    uint32_t max_depth
) const {
    std::vector<std::vector<NodeID>> chains;
    std::vector<NodeID> current_chain;
    std::set<NodeID> visited;
    
    // DFS to find vouching chains
    std::function<void(const NodeID&, uint32_t)> dfs = [&](const NodeID& current, uint32_t depth) {
        if (depth >= max_depth) {
            return;
        }
        
        if (visited.find(current) != visited.end()) {
            return;  // Avoid cycles
        }
        
        visited.insert(current);
        current_chain.push_back(current);
        
        // Get nodes that vouch for current
        auto vouches = reputation_mgr_.get_vouches_by(current);
        if (vouches.empty()) {
            // End of chain
            if (current_chain.size() > 1) {
                chains.push_back(current_chain);
            }
        } else {
            for (const auto& vouch : vouches) {
                if (vouch.still_active) {
                    dfs(vouch.vouchee, depth + 1);
                }
            }
        }
        
        current_chain.pop_back();
        visited.erase(current);
    };
    
    dfs(node, 0);
    
    return chains;
}

bool VouchingWorkflow::has_vouch(const NodeID& voucher, const NodeID& vouchee) const {
    auto vouches = reputation_mgr_.get_vouches_by(voucher);
    
    for (const auto& vouch : vouches) {
        if (vouch.vouchee == vouchee && vouch.still_active) {
            return true;
        }
    }
    
    return false;
}

VouchingWorkflow::VouchingStats VouchingWorkflow::get_vouching_stats(const NodeID& node) const {
    VouchingStats stats{};
    
    // Count vouches by this node
    auto vouches_by = reputation_mgr_.get_vouches_by(node);
    stats.total_vouching = vouches_by.size();
    
    for (const auto& vouch : vouches_by) {
        if (vouch.still_active) {
            stats.active_vouches++;
        } else {
            stats.revoked_vouches++;
        }
    }
    
    // TODO: Count vouches for this node (needs index)
    // TODO: Calculate average voucher reputation
    // TODO: Calculate vouching impact score
    
    return stats;
}

// TrustPathFinder implementation

TrustPathFinder::TrustPathFinder(const TrustGraph& graph)
    : graph_(graph)
{
}

void TrustPathFinder::dfs_find_paths(
    const NodeID& current,
    const NodeID& target,
    std::vector<NodeID>& current_path,
    std::set<NodeID>& visited,
    std::vector<std::vector<NodeID>>& all_paths,
    uint32_t max_hops
) const {
    if (current_path.size() >= max_hops) {
        return;
    }
    
    if (current == target) {
        all_paths.push_back(current_path);
        return;
    }
    
    visited.insert(current);
    
    auto trusted = graph_.get_trusts(current);
    for (const auto& next : trusted) {
        if (visited.find(next) == visited.end()) {
            current_path.push_back(next);
            dfs_find_paths(next, target, current_path, visited, all_paths, max_hops);
            current_path.pop_back();
        }
    }
    
    visited.erase(current);
}

std::vector<std::vector<NodeID>> TrustPathFinder::find_all_paths(
    const NodeID& from,
    const NodeID& to,
    uint32_t max_hops
) const {
    std::vector<std::vector<NodeID>> all_paths;
    std::vector<NodeID> current_path;
    std::set<NodeID> visited;
    
    current_path.push_back(from);
    dfs_find_paths(from, to, current_path, visited, all_paths, max_hops);
    
    return all_paths;
}

std::optional<std::vector<NodeID>> TrustPathFinder::find_strongest_path(
    const NodeID& from,
    const NodeID& to,
    uint32_t max_hops
) const {
    auto all_paths = find_all_paths(from, to, max_hops);
    
    if (all_paths.empty()) {
        return std::nullopt;
    }
    
    // Find path with highest strength
    float max_strength = 0.0f;
    std::vector<NodeID> strongest_path;
    
    for (const auto& path : all_paths) {
        float strength = calculate_path_strength(path);
        if (strength > max_strength) {
            max_strength = strength;
            strongest_path = path;
        }
    }
    
    return strongest_path;
}

float TrustPathFinder::calculate_path_strength(const std::vector<NodeID>& path) const {
    if (path.size() < 2) {
        return 0.0f;
    }
    
    float strength = 1.0f;
    
    for (size_t i = 0; i < path.size() - 1; ++i) {
        auto trust = graph_.get_direct_trust(path[i], path[i + 1]);
        if (!trust) {
            return 0.0f;  // Broken path
        }
        strength *= trust.value();
    }
    
    return strength;
}

std::vector<NodeID> TrustPathFinder::find_common_trusted_nodes(
    const NodeID& node_a,
    const NodeID& node_b,
    float min_trust
) const {
    auto trusted_by_a = graph_.get_trusts(node_a);
    auto trusted_by_b = graph_.get_trusts(node_b);
    
    std::set<NodeID> set_a(trusted_by_a.begin(), trusted_by_a.end());
    std::vector<NodeID> common;
    
    for (const auto& node : trusted_by_b) {
        if (set_a.find(node) != set_a.end()) {
            // Check if trust meets minimum threshold
            auto trust_a = graph_.get_direct_trust(node_a, node);
            auto trust_b = graph_.get_direct_trust(node_b, node);
            
            if (trust_a && trust_b && 
                trust_a.value() >= min_trust && 
                trust_b.value() >= min_trust) {
                common.push_back(node);
            }
        }
    }
    
    return common;
}

std::optional<uint32_t> TrustPathFinder::calculate_trust_distance(
    const NodeID& from,
    const NodeID& to
) const {
    if (from == to) {
        return 0;
    }
    
    // BFS for shortest path
    std::queue<std::pair<NodeID, uint32_t>> queue;
    std::set<NodeID> visited;
    
    queue.push({from, 0});
    visited.insert(from);
    
    while (!queue.empty()) {
        auto [current, distance] = queue.front();
        queue.pop();
        
        if (current == to) {
            return distance;
        }
        
        auto trusted = graph_.get_trusts(current);
        for (const auto& next : trusted) {
            if (visited.find(next) == visited.end()) {
                visited.insert(next);
                queue.push({next, distance + 1});
            }
        }
    }
    
    return std::nullopt;  // No path found
}

std::vector<NodeID> TrustPathFinder::find_bridge_nodes(float min_betweenness) const {
    std::vector<NodeID> bridges;
    
    // Calculate betweenness centrality for all nodes
    // This is computationally expensive for large graphs
    // TODO: Implement efficient betweenness centrality algorithm
    
    return bridges;
}

float TrustPathFinder::calculate_betweenness_centrality(const NodeID& node) const {
    // Betweenness centrality: fraction of shortest paths that pass through this node
    // TODO: Implement Brandes' algorithm for efficient calculation
    
    return 0.0f;
}

std::vector<NodeID> TrustPathFinder::find_trust_hubs(uint32_t min_incoming_edges) const {
    std::map<NodeID, uint32_t> incoming_counts;
    
    // Count incoming edges for each node
    // This requires iterating through all edges
    // TODO: Implement efficient hub finding
    
    std::vector<NodeID> hubs;
    for (const auto& [node, count] : incoming_counts) {
        if (count >= min_incoming_edges) {
            hubs.push_back(node);
        }
    }
    
    return hubs;
}

} // namespace cashew::reputation
