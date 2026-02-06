#include "identity/human_identity.hpp"
#include "crypto/ed25519.hpp"
#include "crypto/blake3.hpp"
#include "utils/logger.hpp"
#include <chrono>
#include <algorithm>

namespace cashew::identity {

// HumanIdentity implementation

HumanIdentity HumanIdentity::create_new() {
    HumanIdentity identity;
    
    // Generate new key pair
    auto [public_key, secret_key] = crypto::Ed25519::generate_keypair();
    identity.public_key_ = public_key;
    
    // Generate ID from public key
    identity.id_ = HumanID(crypto::Blake3::hash(
        std::vector<uint8_t>(public_key.begin(), public_key.end())
    ));
    
    identity.created_at_ = identity.current_timestamp();
    
    CASHEW_LOG_INFO("Created new human identity: {}", identity.id_.to_string().substr(0, 16));
    
    return identity;
}

std::optional<HumanIdentity> HumanIdentity::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 32 + 32) {
        return std::nullopt;
    }
    
    HumanIdentity identity;
    size_t offset = 0;
    
    // Created timestamp (8 bytes)
    identity.created_at_ = 0;
    for (size_t i = 0; i < 8; i++) {
        identity.created_at_ |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Public key (32 bytes)
    std::copy(data.begin() + offset, data.begin() + offset + 32, identity.public_key_.begin());
    offset += 32;
    
    // ID (32 bytes)
    Hash256 id_hash;
    std::copy(data.begin() + offset, data.begin() + offset + 32, id_hash.begin());
    identity.id_ = HumanID(id_hash);
    offset += 32;
    
    // Display name (optional, length-prefixed)
    if (offset < data.size()) {
        uint32_t name_len = 0;
        for (size_t i = 0; i < 4 && offset + i < data.size(); i++) {
            name_len |= static_cast<uint32_t>(data[offset + i]) << (i * 8);
        }
        offset += 4;
        
        if (name_len > 0 && offset + name_len <= data.size()) {
            std::string name(data.begin() + offset, data.begin() + offset + name_len);
            identity.display_name_ = name;
            offset += name_len;
        }
    }
    
    return identity;
}

void HumanIdentity::set_display_name(const std::string& name) {
    if (name.length() > 256) {
        CASHEW_LOG_WARN("Display name too long, truncating to 256 chars");
        display_name_ = name.substr(0, 256);
    } else {
        display_name_ = name;
    }
}

void HumanIdentity::associate_node(const NodeID& node_id) {
    associated_nodes_.insert(node_id);
    CASHEW_LOG_DEBUG("Associated node {} with human identity {}",
                    cashew::hash_to_hex(node_id.id).substr(0, 16),
                    id_.to_string().substr(0, 16));
}

void HumanIdentity::disassociate_node(const NodeID& node_id) {
    associated_nodes_.erase(node_id);
}

std::vector<NodeID> HumanIdentity::get_associated_nodes() const {
    return std::vector<NodeID>(associated_nodes_.begin(), associated_nodes_.end());
}

bool HumanIdentity::is_associated_with(const NodeID& node_id) const {
    return associated_nodes_.find(node_id) != associated_nodes_.end();
}

void HumanIdentity::link_reputation(const reputation::ReputationScore& score) {
    reputation_ = score;
}

void HumanIdentity::add_attestation(const Attestation& attestation) {
    if (!attestation.is_valid()) {
        CASHEW_LOG_WARN("Rejecting invalid attestation");
        return;
    }
    
    if (attestation.is_expired()) {
        CASHEW_LOG_WARN("Rejecting expired attestation");
        return;
    }
    
    attestations_.push_back(attestation);
    CASHEW_LOG_INFO("Added attestation from {} to {}", 
                   attestation.attester_id.to_string().substr(0, 16),
                   id_.to_string().substr(0, 16));
}

bool HumanIdentity::Attestation::is_valid() const {
    // Basic validation
    if (timestamp == 0) return false;
    if (claim.empty()) return false;
    if (expires_at <= timestamp) return false;
    
    return true;
}

bool HumanIdentity::Attestation::is_expired() const {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return expires_at < static_cast<uint64_t>(now);
}

HumanIdentity::ContinuityProof HumanIdentity::create_continuity_proof(
    const Hash256& challenge,
    const SecretKey& secret_key
) {
    ContinuityProof proof;
    proof.challenge = challenge;
    proof.timestamp = current_timestamp();
    
    // Build message to sign: challenge + timestamp
    std::vector<uint8_t> message;
    message.insert(message.end(), challenge.begin(), challenge.end());
    for (size_t i = 0; i < 8; i++) {
        message.push_back(static_cast<uint8_t>(proof.timestamp >> (i * 8)));
    }
    
    // Sign
    proof.signature = crypto::Ed25519::sign(message, secret_key);
    
    return proof;
}

bool HumanIdentity::verify_continuity(const ContinuityProof& proof) const {
    // Rebuild message
    std::vector<uint8_t> message;
    message.insert(message.end(), proof.challenge.begin(), proof.challenge.end());
    for (size_t i = 0; i < 8; i++) {
        message.push_back(static_cast<uint8_t>(proof.timestamp >> (i * 8)));
    }
    
    // Verify signature
    return crypto::Ed25519::verify(message, proof.signature, public_key_);
}

bool HumanIdentity::ContinuityProof::verify(const PublicKey& expected_key) const {
    // Rebuild message
    std::vector<uint8_t> message;
    message.insert(message.end(), challenge.begin(), challenge.end());
    for (size_t i = 0; i < 8; i++) {
        message.push_back(static_cast<uint8_t>(timestamp >> (i * 8)));
    }
    
    return crypto::Ed25519::verify(message, signature, expected_key);
}

Signature HumanIdentity::sign_message(
    const std::vector<uint8_t>& message,
    const SecretKey& secret_key
) const {
    return crypto::Ed25519::sign(message, secret_key);
}

bool HumanIdentity::verify_signature(
    const std::vector<uint8_t>& message,
    const Signature& signature
) const {
    return crypto::Ed25519::verify(message, signature, public_key_);
}

std::vector<uint8_t> HumanIdentity::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Created timestamp (8 bytes)
    for (size_t i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(created_at_ >> (i * 8)));
    }
    
    // Public key (32 bytes)
    data.insert(data.end(), public_key_.begin(), public_key_.end());
    
    // ID (32 bytes)
    data.insert(data.end(), id_.id.begin(), id_.id.end());
    
    // Display name (optional, length-prefixed)
    if (display_name_) {
        uint32_t name_len = static_cast<uint32_t>(display_name_->length());
        for (size_t i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(name_len >> (i * 8)));
        }
        data.insert(data.end(), display_name_->begin(), display_name_->end());
    } else {
        // Zero length
        for (size_t i = 0; i < 4; i++) {
            data.push_back(0);
        }
    }
    
    return data;
}

uint64_t HumanIdentity::current_timestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// HumanIdentityManager implementation

HumanIdentity HumanIdentityManager::create_identity() {
    HumanIdentity identity = HumanIdentity::create_new();
    register_identity(identity);
    return identity;
}

void HumanIdentityManager::register_identity(const HumanIdentity& identity) {
    identities_[identity.get_id()] = identity;
    
    // Map associated nodes
    for (const auto& node_id : identity.get_associated_nodes()) {
        node_to_human_[node_id] = identity.get_id();
    }
    
    CASHEW_LOG_INFO("Registered human identity: {}", 
                   identity.get_id().to_string().substr(0, 16));
}

void HumanIdentityManager::unregister_identity(const HumanID& human_id) {
    auto it = identities_.find(human_id);
    if (it == identities_.end()) {
        return;
    }
    
    // Remove node mappings
    for (const auto& node_id : it->second.get_associated_nodes()) {
        node_to_human_.erase(node_id);
    }
    
    identities_.erase(it);
}

std::optional<HumanIdentity> HumanIdentityManager::get_identity(const HumanID& human_id) const {
    auto it = identities_.find(human_id);
    if (it == identities_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<HumanIdentity> HumanIdentityManager::get_identity_by_node(const NodeID& node_id) const {
    auto it = node_to_human_.find(node_id);
    if (it == node_to_human_.end()) {
        return std::nullopt;
    }
    
    return get_identity(it->second);
}

bool HumanIdentityManager::add_attestation(
    const HumanID& target_id,
    const HumanIdentity::Attestation& attestation
) {
    auto it = identities_.find(target_id);
    if (it == identities_.end()) {
        return false;
    }
    
    it->second.add_attestation(attestation);
    return true;
}

std::vector<HumanIdentity::Attestation> HumanIdentityManager::get_attestations_for(
    const HumanID& human_id
) const {
    auto it = identities_.find(human_id);
    if (it == identities_.end()) {
        return {};
    }
    
    return it->second.get_attestations();
}

std::optional<HumanIdentityManager::ImpersonationAlert> HumanIdentityManager::detect_impersonation(
    const HumanID& claimed_id,
    const NodeID& node_id,
    const HumanIdentity::ContinuityProof& proof
) {
    auto identity = get_identity(claimed_id);
    if (!identity) {
        ImpersonationAlert alert;
        alert.claimed_id = claimed_id;
        alert.suspicious_node = node_id;
        alert.reason = "Unknown identity claimed";
        alert.detected_at = current_timestamp();
        
        alerts_.push_back(alert);
        return alert;
    }
    
    // Verify continuity proof
    if (!identity->verify_continuity(proof)) {
        ImpersonationAlert alert;
        alert.claimed_id = claimed_id;
        alert.suspicious_node = node_id;
        alert.reason = "Failed continuity verification";
        alert.detected_at = current_timestamp();
        
        alerts_.push_back(alert);
        
        CASHEW_LOG_WARN("Impersonation attempt detected: node {} claiming to be {}",
                       cashew::hash_to_hex(node_id.id).substr(0, 16),
                       claimed_id.to_string().substr(0, 16));
        
        return alert;
    }
    
    return std::nullopt;
}

void HumanIdentityManager::add_trust_edge(const HumanID& from, const HumanID& to, float weight) {
    TrustEdge edge;
    edge.from = from;
    edge.to = to;
    edge.weight = std::clamp(weight, 0.0f, 1.0f);
    edge.created_at = current_timestamp();
    
    trust_graph_.insert({from, edge});
    
    CASHEW_LOG_DEBUG("Added trust edge: {} -> {} (weight={})",
                    from.to_string().substr(0, 16),
                    to.to_string().substr(0, 16),
                    weight);
}

std::vector<HumanIdentityManager::TrustEdge> HumanIdentityManager::get_trust_edges(
    const HumanID& human_id
) const {
    std::vector<TrustEdge> edges;
    
    auto range = trust_graph_.equal_range(human_id);
    for (auto it = range.first; it != range.second; ++it) {
        edges.push_back(it->second);
    }
    
    return edges;
}

size_t HumanIdentityManager::attestation_count() const {
    size_t count = 0;
    for (const auto& [id, identity] : identities_) {
        count += identity.attestation_count();
    }
    return count;
}

uint64_t HumanIdentityManager::current_timestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool HumanIdentityManager::verify_attestation_chain(const HumanID& human_id) const {
    auto identity = get_identity(human_id);
    if (!identity) {
        return false;
    }
    
    // Check if any attestations exist
    if (identity->attestation_count() == 0) {
        return false;
    }
    
    // Verify all attestations are valid
    for (const auto& attestation : identity->get_attestations()) {
        if (!attestation.is_valid() || attestation.is_expired()) {
            return false;
        }
    }
    
    return true;
}

} // namespace cashew::identity
