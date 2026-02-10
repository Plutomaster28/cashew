#include "security/token_revocation.hpp"
#include "core/node/node_identity.hpp"
#include "crypto/blake3.hpp"
#include "crypto/ed25519.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace cashew::security {

// TokenRevocation methods

bool TokenRevocation::matches_token(const CapabilityToken& token) const {
    // Check if revocation applies to this token
    if (token.node_id.id != node_id.id) {
        return false;
    }
    
    if (token.capability != capability) {
        return false;
    }
    
    // Check context match if revocation has context
    if (!context.empty() && !token.context.empty()) {
        if (context != token.context) {
            return false;
        }
    }
    
    // Check if token was issued before revocation
    if (token.issued_at >= revoked_at) {
        return false;  // Token issued after revocation, not affected
    }
    
    return true;
}

std::vector<uint8_t> TokenRevocation::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Node ID (32 bytes)
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    
    // Capability (1 byte)
    data.push_back(static_cast<uint8_t>(capability));
    
    // Reason (1 byte)
    data.push_back(static_cast<uint8_t>(reason));
    
    // Revoked at (8 bytes)
    uint64_t rev_at = revoked_at;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(rev_at >> (i * 8)));
    }
    
    // Revoker (32 bytes)
    data.insert(data.end(), revoker.id.begin(), revoker.id.end());
    
    // Context (length-prefixed)
    uint32_t ctx_len = static_cast<uint32_t>(context.size());
    for (int i = 0; i < 4; i++) {
        data.push_back(static_cast<uint8_t>(ctx_len >> (i * 8)));
    }
    data.insert(data.end(), context.begin(), context.end());
    
    // Signature (64 bytes)
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<TokenRevocation> TokenRevocation::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 32 + 1 + 1 + 8 + 32 + 4 + 64) {
        return std::nullopt;
    }
    
    TokenRevocation rev;
    size_t offset = 0;
    
    // Node ID
    std::copy(data.begin(), data.begin() + 32, rev.node_id.id.begin());
    offset += 32;
    
    // Capability
    rev.capability = static_cast<Capability>(data[offset++]);
    
    // Reason
    rev.reason = static_cast<RevocationReason>(data[offset++]);
    
    // Revoked at
    rev.revoked_at = 0;
    for (int i = 0; i < 8; i++) {
        rev.revoked_at |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Revoker
    std::copy(data.begin() + offset, data.begin() + offset + 32, rev.revoker.id.begin());
    offset += 32;
    
    // Context length
    uint32_t ctx_len = 0;
    for (int i = 0; i < 4; i++) {
        ctx_len |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    if (data.size() < offset + ctx_len + 64) {
        return std::nullopt;
    }
    
    // Context
    if (ctx_len > 0) {
        rev.context.assign(data.begin() + offset, data.begin() + offset + ctx_len);
        offset += ctx_len;
    }
    
    // Signature
    std::copy(data.begin() + offset, data.begin() + offset + 64, rev.signature.begin());
    
    return rev;
}

Hash256 TokenRevocation::get_id() const {
    // Create unique ID by hashing the revocation data (excluding signature)
    std::vector<uint8_t> data;
    
    data.insert(data.end(), node_id.id.begin(), node_id.id.end());
    data.push_back(static_cast<uint8_t>(capability));
    data.push_back(static_cast<uint8_t>(reason));
    
    uint64_t rev_at = revoked_at;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(rev_at >> (i * 8)));
    }
    
    data.insert(data.end(), revoker.id.begin(), revoker.id.end());
    data.insert(data.end(), context.begin(), context.end());
    
    return crypto::Blake3::hash(data);
}

// RevocationListUpdate methods

std::vector<uint8_t> RevocationListUpdate::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Timestamp (8 bytes)
    uint64_t ts = timestamp;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(ts >> (i * 8)));
    }
    
    // Source node (32 bytes)
    data.insert(data.end(), source_node.id.begin(), source_node.id.end());
    
    // Number of revocations (4 bytes)
    uint32_t count = static_cast<uint32_t>(revocations.size());
    for (int i = 0; i < 4; i++) {
        data.push_back(static_cast<uint8_t>(count >> (i * 8)));
    }
    
    // Revocations
    for (const auto& rev : revocations) {
        auto rev_bytes = rev.to_bytes();
        uint32_t len = static_cast<uint32_t>(rev_bytes.size());
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(len >> (i * 8)));
        }
        data.insert(data.end(), rev_bytes.begin(), rev_bytes.end());
    }
    
    // Signature (64 bytes)
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<RevocationListUpdate> RevocationListUpdate::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 32 + 4 + 64) {
        return std::nullopt;
    }
    
    RevocationListUpdate update;
    size_t offset = 0;
    
    // Timestamp
    update.timestamp = 0;
    for (int i = 0; i < 8; i++) {
        update.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Source node
    std::copy(data.begin() + offset, data.begin() + offset + 32, update.source_node.id.begin());
    offset += 32;
    
    // Number of revocations
    uint32_t count = 0;
    for (int i = 0; i < 4; i++) {
        count |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    // Read revocations
    for (uint32_t i = 0; i < count; i++) {
        if (data.size() < offset + 4) {
            return std::nullopt;
        }
        
        uint32_t len = 0;
        for (int j = 0; j < 4; j++) {
            len |= static_cast<uint32_t>(data[offset++]) << (j * 8);
        }
        
        if (data.size() < offset + len) {
            return std::nullopt;
        }
        
        std::vector<uint8_t> rev_data(data.begin() + offset, data.begin() + offset + len);
        auto rev = TokenRevocation::from_bytes(rev_data);
        if (!rev) {
            return std::nullopt;
        }
        
        update.revocations.push_back(*rev);
        offset += len;
    }
    
    // Signature
    if (data.size() < offset + 64) {
        return std::nullopt;
    }
    std::copy(data.begin() + offset, data.begin() + offset + 64, update.signature.begin());
    
    return update;
}

// TokenRevocationManager methods

TokenRevocationManager::TokenRevocationManager()
    : revocation_expiry_days_(30)
    , max_revocations_per_node_(100)
    , max_propagation_count_(10)
{
    CASHEW_LOG_INFO("TokenRevocationManager initialized");
}

std::optional<TokenRevocation> TokenRevocationManager::revoke_token(
    const NodeID& node_id,
    Capability capability,
    RevocationReason reason,
    const NodeID& revoker,
    const std::vector<uint8_t>& context
) {
    TokenRevocation revocation;
    revocation.node_id = node_id;
    revocation.capability = capability;
    revocation.reason = reason;
    revocation.revoked_at = current_timestamp();
    revocation.revoker = revoker;
    revocation.context = context;
    // Note: Signature would be added by sign_revocation()
    
    // Check if we already have too many revocations for this node
    auto it = revocations_by_node_.find(node_id);
    if (it != revocations_by_node_.end() && it->second.size() >= max_revocations_per_node_) {
        CASHEW_LOG_WARN("Too many revocations for node, rejecting new revocation");
        return std::nullopt;
    }
    
    // Add to storage
    Hash256 id = revocation.get_id();
    
    RevocationListEntry entry(revocation);
    revocations_[id] = entry;
    
    add_revocation_to_indexes(id, revocation);
    
    CASHEW_LOG_INFO("Token revoked for node {} capability {} reason {}",
                    node_id.to_string().substr(0, 8),
                    static_cast<int>(capability),
                    revocation_reason_to_string(reason));
    
    return revocation;
}

bool TokenRevocationManager::is_token_revoked(const CapabilityToken& token) const {
    // Check all revocations for this node
    auto it = revocations_by_node_.find(token.node_id);
    if (it == revocations_by_node_.end()) {
        return false;
    }
    
    // Check each revocation to see if it matches this token
    for (const auto& rev_id : it->second) {
        auto rev_it = revocations_.find(rev_id);
        if (rev_it != revocations_.end()) {
            if (rev_it->second.revocation.matches_token(token)) {
                return true;
            }
        }
    }
    
    return false;
}

bool TokenRevocationManager::has_revocations(const NodeID& node_id, Capability capability) const {
    auto it = revocations_by_node_.find(node_id);
    if (it == revocations_by_node_.end()) {
        return false;
    }
    
    for (const auto& rev_id : it->second) {
        auto rev_it = revocations_.find(rev_id);
        if (rev_it != revocations_.end()) {
            if (rev_it->second.revocation.capability == capability) {
                return true;
            }
        }
    }
    
    return false;
}

std::vector<TokenRevocation> TokenRevocationManager::get_revocations_for(const NodeID& node_id) const {
    std::vector<TokenRevocation> result;
    
    auto it = revocations_by_node_.find(node_id);
    if (it == revocations_by_node_.end()) {
        return result;
    }
    
    for (const auto& rev_id : it->second) {
        auto rev_it = revocations_.find(rev_id);
        if (rev_it != revocations_.end()) {
            result.push_back(rev_it->second.revocation);
        }
    }
    
    return result;
}

std::vector<TokenRevocation> TokenRevocationManager::get_recent_revocations(
    uint64_t since_timestamp,
    size_t max_count
) const {
    std::vector<TokenRevocation> result;
    
    for (const auto& [id, entry] : revocations_) {
        if (entry.revocation.revoked_at >= since_timestamp) {
            result.push_back(entry.revocation);
            
            if (result.size() >= max_count) {
                break;
            }
        }
    }
    
    return result;
}

bool TokenRevocationManager::process_revocation(
    const TokenRevocation& revocation,
    const NodeID& source_node
) {
    Hash256 id = revocation.get_id();
    
    // Check if we've already seen this revocation
    if (seen_revocations_.count(id) > 0) {
        return false;  // Already processed
    }
    
    // Validate revocation
    if (!should_accept_revocation(revocation)) {
        CASHEW_LOG_WARN("Rejecting invalid revocation from {}", source_node.to_string().substr(0, 8));
        return false;
    }
    
    // Add to storage
    RevocationListEntry entry(revocation);
    entry.witnesses.insert(source_node);
    
    revocations_[id] = entry;
    seen_revocations_.insert(id);
    
    add_revocation_to_indexes(id, revocation);
    
    CASHEW_LOG_INFO("Processed revocation from source node");
    
    return true;
}

size_t TokenRevocationManager::process_revocation_list(const RevocationListUpdate& update) {
    size_t accepted = 0;
    
    for (const auto& revocation : update.revocations) {
        if (process_revocation(revocation, update.source_node)) {
            accepted++;
        }
    }
    
    CASHEW_LOG_INFO("Processed revocation list: {}/{} accepted",
                    accepted, update.revocations.size());
    
    return accepted;
}

RevocationListUpdate TokenRevocationManager::create_revocation_list(bool include_all) const {
    RevocationListUpdate update;
    update.timestamp = current_timestamp();
    // Note: source_node and signature would be filled in by caller
    
    if (include_all) {
        // Include all revocations
        for (const auto& [id, entry] : revocations_) {
            update.revocations.push_back(entry.revocation);
        }
    } else {
        // Only recent (last hour)
        uint64_t one_hour_ago = current_timestamp() - 3600;
        update.revocations = get_recent_revocations(one_hour_ago, 100);
    }
    
    return update;
}

bool TokenRevocationManager::verify_revocation(
    const TokenRevocation& revocation,
    const PublicKey& revoker_public_key
) {
    // Serialize revocation data for verification (excluding signature)
    std::vector<uint8_t> data;
    
    data.insert(data.end(), revocation.node_id.id.begin(), revocation.node_id.id.end());
    data.push_back(static_cast<uint8_t>(revocation.capability));
    data.push_back(static_cast<uint8_t>(revocation.reason));
    
    uint64_t rev_at = revocation.revoked_at;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(rev_at >> (i * 8)));
    }
    
    data.insert(data.end(), revocation.revoker.id.begin(), revocation.revoker.id.end());
    data.insert(data.end(), revocation.context.begin(), revocation.context.end());
    
    // Verify Ed25519 signature
    return crypto::Ed25519::verify(data, revocation.signature, revoker_public_key);
}

void TokenRevocationManager::sign_revocation(
    TokenRevocation& revocation,
    const Signature& signature
) {
    // Simply apply the provided signature
    revocation.signature = signature;
}

size_t TokenRevocationManager::revocation_count() const {
    return revocations_.size();
}

size_t TokenRevocationManager::expired_revocation_count() const {
    size_t count = 0;
    for (const auto& [id, entry] : revocations_) {
        if (is_revocation_expired(entry.revocation)) {
            count++;
        }
    }
    return count;
}

void TokenRevocationManager::cleanup_expired_revocations() {
    std::vector<Hash256> to_remove;
    
    for (const auto& [id, entry] : revocations_) {
        if (is_revocation_expired(entry.revocation)) {
            to_remove.push_back(id);
        }
    }
    
    for (const auto& id : to_remove) {
        auto it = revocations_.find(id);
        if (it != revocations_.end()) {
            remove_revocation_from_indexes(id, it->second.revocation);
            revocations_.erase(it);
        }
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_INFO("Cleaned up {} expired revocations", to_remove.size());
    }
}

// Private methods

bool TokenRevocationManager::is_revocation_expired(const TokenRevocation& revocation) const {
    uint64_t expiry_seconds = revocation_expiry_days_ * 24 * 3600;
    uint64_t expiry_time = revocation.revoked_at + expiry_seconds;
    return current_timestamp() >= expiry_time;
}

bool TokenRevocationManager::should_accept_revocation(const TokenRevocation& revocation) const {
    // Check if revocation is too old
    if (is_revocation_expired(revocation)) {
        return false;
    }
    
    // Check if revocation is from the future (clock skew tolerance: 5 minutes)
    uint64_t now = current_timestamp();
    if (revocation.revoked_at > now + 300) {
        return false;
    }
    
    // Check if node already has too many revocations
    auto it = revocations_by_node_.find(revocation.node_id);
    if (it != revocations_by_node_.end() && it->second.size() >= max_revocations_per_node_) {
        return false;
    }
    
    return true;
}

void TokenRevocationManager::add_revocation_to_indexes(
    const Hash256& id,
    const TokenRevocation& revocation
) {
    revocations_by_node_[revocation.node_id].insert(id);
    revocations_by_capability_[revocation.capability].insert(id);
}

void TokenRevocationManager::remove_revocation_from_indexes(
    const Hash256& id,
    const TokenRevocation& revocation
) {
    auto node_it = revocations_by_node_.find(revocation.node_id);
    if (node_it != revocations_by_node_.end()) {
        node_it->second.erase(id);
        if (node_it->second.empty()) {
            revocations_by_node_.erase(node_it);
        }
    }
    
    auto cap_it = revocations_by_capability_.find(revocation.capability);
    if (cap_it != revocations_by_capability_.end()) {
        cap_it->second.erase(id);
        if (cap_it->second.empty()) {
            revocations_by_capability_.erase(cap_it);
        }
    }
    
    seen_revocations_.erase(id);
}

uint64_t TokenRevocationManager::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::system_clock::to_time_t(now);
}

// Helper functions

const char* revocation_reason_to_string(RevocationReason reason) {
    switch (reason) {
        case RevocationReason::MANUAL_REVOCATION: return "Manual revocation";
        case RevocationReason::COMPROMISED_KEY: return "Compromised key";
        case RevocationReason::POLICY_VIOLATION: return "Policy violation";
        case RevocationReason::ABUSE_DETECTED: return "Abuse detected";
        case RevocationReason::EXPIRED_CREDENTIALS: return "Expired credentials";
        case RevocationReason::NETWORK_REMOVAL: return "Network removal";
        case RevocationReason::REPUTATION_LOSS: return "Reputation loss";
        default: return "Unknown";
    }
}

} // namespace cashew::security
