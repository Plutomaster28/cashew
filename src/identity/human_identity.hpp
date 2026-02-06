#pragma once

#include "cashew/common.hpp"
#include "core/keys/key.hpp"
#include "core/reputation/reputation.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <string>

namespace cashew::identity {

/**
 * HumanIdentity - Pseudonymous identity for human users
 * 
 * Sits above NodeID, providing:
 * - Persistent reputation across node changes
 * - Human-readable attributes
 * - Attestation management
 * - Continuity verification
 * - Anti-impersonation protection
 */
class HumanIdentity {
public:
    HumanIdentity() = default;
    
    // Create new identity with fresh keys
    static HumanIdentity create_new();
    
    // Load from stored data
    static std::optional<HumanIdentity> from_bytes(const std::vector<uint8_t>& data);
    
    // Identity info
    HumanID get_id() const { return id_; }
    PublicKey get_public_key() const { return public_key_; }
    uint64_t created_at() const { return created_at_; }
    
    // Display name (optional)
    void set_display_name(const std::string& name);
    std::optional<std::string> get_display_name() const { return display_name_; }
    
    // Associated nodes
    void associate_node(const NodeID& node_id);
    void disassociate_node(const NodeID& node_id);
    std::vector<NodeID> get_associated_nodes() const;
    bool is_associated_with(const NodeID& node_id) const;
    
    // Reputation linking
    void link_reputation(const reputation::ReputationScore& score);
    std::optional<reputation::ReputationScore> get_reputation() const { return reputation_; }
    
    // Attestations (vouches from other humans)
    struct Attestation {
        HumanID attester_id;
        Signature signature;
        uint64_t timestamp;
        std::string claim;  // What they attest to
        uint64_t expires_at;
        
        bool is_valid() const;
        bool is_expired() const;
    };
    
    void add_attestation(const Attestation& attestation);
    std::vector<Attestation> get_attestations() const { return attestations_; }
    size_t attestation_count() const { return attestations_.size(); }
    
    // Continuity verification (prove you still control this identity)
    struct ContinuityProof {
        Hash256 challenge;
        Signature signature;
        uint64_t timestamp;
        NodeID node_id;  // Which node signed this
        
        bool verify(const PublicKey& expected_key) const;
    };
    
    ContinuityProof create_continuity_proof(const Hash256& challenge, const SecretKey& secret_key);
    bool verify_continuity(const ContinuityProof& proof) const;
    
    // Signing
    Signature sign_message(const std::vector<uint8_t>& message, const SecretKey& secret_key) const;
    bool verify_signature(const std::vector<uint8_t>& message, const Signature& signature) const;
    
    // Serialization
    std::vector<uint8_t> to_bytes() const;
    
private:
    HumanID id_;
    PublicKey public_key_;
    uint64_t created_at_;
    
    std::optional<std::string> display_name_;
    
    std::set<NodeID> associated_nodes_;
    std::optional<reputation::ReputationScore> reputation_;
    std::vector<Attestation> attestations_;
    
    uint64_t current_timestamp() const;
};

/**
 * HumanIdentityManager - Manages human identities
 */
class HumanIdentityManager {
public:
    HumanIdentityManager() = default;
    ~HumanIdentityManager() = default;
    
    // Create and register new identity
    HumanIdentity create_identity();
    
    // Registration
    void register_identity(const HumanIdentity& identity);
    void unregister_identity(const HumanID& human_id);
    
    // Lookup
    std::optional<HumanIdentity> get_identity(const HumanID& human_id) const;
    std::optional<HumanIdentity> get_identity_by_node(const NodeID& node_id) const;
    
    // Attestation management
    bool add_attestation(const HumanID& target_id, const HumanIdentity::Attestation& attestation);
    std::vector<HumanIdentity::Attestation> get_attestations_for(const HumanID& human_id) const;
    
    // Anti-impersonation
    struct ImpersonationAlert {
        HumanID claimed_id;
        NodeID suspicious_node;
        std::string reason;
        uint64_t detected_at;
    };
    
    std::optional<ImpersonationAlert> detect_impersonation(
        const HumanID& claimed_id,
        const NodeID& node_id,
        const HumanIdentity::ContinuityProof& proof
    );
    
    // Trust graph
    struct TrustEdge {
        HumanID from;
        HumanID to;
        float weight;  // 0.0 to 1.0
        uint64_t created_at;
    };
    
    void add_trust_edge(const HumanID& from, const HumanID& to, float weight);
    std::vector<TrustEdge> get_trust_edges(const HumanID& human_id) const;
    
    // Statistics
    size_t identity_count() const { return identities_.size(); }
    size_t attestation_count() const;
    
private:
    std::map<HumanID, HumanIdentity> identities_;
    std::map<NodeID, HumanID> node_to_human_;
    std::multimap<HumanID, TrustEdge> trust_graph_;
    
    std::vector<ImpersonationAlert> alerts_;
    
    uint64_t current_timestamp() const;
    bool verify_attestation_chain(const HumanID& human_id) const;
};

} // namespace cashew::identity
