#pragma once

#include "cashew/common.hpp"
#include "reputation.hpp"
#include "core/node/node_identity.hpp"
#include "crypto/ed25519.hpp"
#include <optional>
#include <vector>

namespace cashew::reputation {

/**
 * AttestationSigner - Handles cryptographic signing and verification of attestations
 */
class AttestationSigner {
public:
    /**
     * Sign an attestation with the attester's identity
     */
    static bool sign_attestation(
        Attestation& attestation,
        const core::NodeIdentity& attester_identity
    );
    
    /**
     * Verify an attestation signature
     */
    static bool verify_attestation_signature(
        const Attestation& attestation,
        const PublicKey& attester_public_key
    );
    
    /**
     * Serialize attestation for signing (without signature field)
     */
    static std::vector<uint8_t> attestation_to_signable_bytes(
        const Attestation& attestation
    );
    
    /**
     * Create a vouch attestation (positive endorsement)
     */
    static Attestation create_vouch_attestation(
        const NodeID& voucher,
        const NodeID& vouchee,
        int32_t score_boost,
        const std::string& statement,
        uint64_t validity_duration_seconds = 2592000  // 30 days default
    );
    
    /**
     * Create a denouncement attestation (negative feedback)
     */
    static Attestation create_denouncement_attestation(
        const NodeID& denouncer,
        const NodeID& subject,
        int32_t score_penalty,
        const std::string& reason,
        uint64_t validity_duration_seconds = 864000  // 10 days default
    );
    
    /**
     * Create a general attestation
     */
    static Attestation create_attestation(
        const NodeID& attester,
        const NodeID& subject,
        int32_t score_delta,
        const std::string& statement,
        uint64_t validity_duration_seconds
    );
};

/**
 * VouchingWorkflow - High-level vouching operations
 */
class VouchingWorkflow {
public:
    explicit VouchingWorkflow(ReputationManager& reputation_mgr);
    ~VouchingWorkflow() = default;
    
    /**
     * Request to vouch for another node
     * Returns true if vouch was successfully created and signed
     */
    bool request_vouch(
        const core::NodeIdentity& voucher_identity,
        const NodeID& vouchee,
        const std::string& reason
    );
    
    /**
     * Accept a vouch from another node
     * The vouchee must acknowledge and sign acceptance
     */
    bool accept_vouch(
        const core::NodeIdentity& vouchee_identity,
        const NodeID& voucher
    );
    
    /**
     * Revoke a vouch (voucher withdraws endorsement)
     */
    bool revoke_vouch(
        const NodeID& voucher,
        const NodeID& vouchee,
        const std::string& reason
    );
    
    /**
     * Get all active vouches for a node (as vouchee)
     */
    std::vector<VouchRecord> get_active_vouches_for(const NodeID& vouchee) const;
    
    /**
     * Get all active vouches by a node (as voucher)
     */
    std::vector<VouchRecord> get_active_vouches_by(const NodeID& voucher) const;
    
    /**
     * Calculate vouching chain (who vouches for whom, recursively)
     */
    std::vector<std::vector<NodeID>> get_vouching_chains(
        const NodeID& node,
        uint32_t max_depth = 3
    ) const;
    
    /**
     * Check if a vouching relationship exists
     */
    bool has_vouch(const NodeID& voucher, const NodeID& vouchee) const;
    
    /**
     * Get vouching statistics for a node
     */
    struct VouchingStats {
        uint32_t total_vouched_by;    // How many vouch for this node
        uint32_t total_vouching;      // How many this node vouches for
        uint32_t active_vouches;      // Currently active vouches
        uint32_t revoked_vouches;     // Vouches that were revoked
        float average_voucher_reputation;  // Avg reputation of vouchers
        float vouching_impact_score;  // Total impact from vouches
    };
    
    VouchingStats get_vouching_stats(const NodeID& node) const;
    
private:
    ReputationManager& reputation_mgr_;
};

/**
 * TrustPathFinder - Advanced trust graph traversal
 */
class TrustPathFinder {
public:
    explicit TrustPathFinder(const TrustGraph& graph);
    ~TrustPathFinder() = default;
    
    /**
     * Find all paths between two nodes within max_hops
     */
    std::vector<std::vector<NodeID>> find_all_paths(
        const NodeID& from,
        const NodeID& to,
        uint32_t max_hops = 5
    ) const;
    
    /**
     * Find the strongest trust path (highest cumulative trust weight)
     */
    std::optional<std::vector<NodeID>> find_strongest_path(
        const NodeID& from,
        const NodeID& to,
        uint32_t max_hops = 5
    ) const;
    
    /**
     * Calculate path strength (product of edge weights along path)
     */
    float calculate_path_strength(const std::vector<NodeID>& path) const;
    
    /**
     * Find common trusted nodes (nodes trusted by both A and B)
     */
    std::vector<NodeID> find_common_trusted_nodes(
        const NodeID& node_a,
        const NodeID& node_b,
        float min_trust = 0.5f
    ) const;
    
    /**
     * Calculate trust distance (shortest path length)
     */
    std::optional<uint32_t> calculate_trust_distance(
        const NodeID& from,
        const NodeID& to
    ) const;
    
    /**
     * Find trust bridge nodes (nodes that connect different communities)
     */
    std::vector<NodeID> find_bridge_nodes(float min_betweenness = 0.5f) const;
    
    /**
     * Calculate betweenness centrality for a node
     * (how many shortest paths pass through this node)
     */
    float calculate_betweenness_centrality(const NodeID& node) const;
    
    /**
     * Find trust hubs (nodes with high in-degree)
     */
    std::vector<NodeID> find_trust_hubs(uint32_t min_incoming_edges = 5) const;
    
private:
    const TrustGraph& graph_;
    
    // Helper for DFS path finding
    void dfs_find_paths(
        const NodeID& current,
        const NodeID& target,
        std::vector<NodeID>& current_path,
        std::set<NodeID>& visited,
        std::vector<std::vector<NodeID>>& all_paths,
        uint32_t max_hops
    ) const;
};

} // namespace cashew::reputation
