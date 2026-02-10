#include "security/key_revocation.hpp"
#include "crypto/blake3.hpp"
#include "crypto/ed25519.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace cashew::security {

// KeyRevocation methods

bytes KeyRevocation::to_bytes() const {
    bytes data;
    
    // Revoked key (32 bytes)
    data.insert(data.end(), revoked_key.begin(), revoked_key.end());
    
    // Reason (1 byte)
    data.push_back(static_cast<uint8_t>(reason));
    
    // Revoked at (8 bytes)
    uint64_t ts = revoked_at;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(ts >> (i * 8)));
    }
    
    // Revoker (32 bytes)
    data.insert(data.end(), revoker.id.begin(), revoker.id.end());
    
    // Replacement key (1 byte flag + 32 bytes if present)
    if (replacement_key) {
        data.push_back(1);  // Has replacement
        data.insert(data.end(), replacement_key->begin(), replacement_key->end());
    } else {
        data.push_back(0);  // No replacement
    }
    
    // Rotation certificate (1 byte flag + cert if present)
    if (rotation_cert) {
        data.push_back(1);  // Has rotation cert
        auto cert_bytes = rotation_cert->to_bytes();
        uint32_t cert_len = static_cast<uint32_t>(cert_bytes.size());
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(cert_len >> (i * 8)));
        }
        data.insert(data.end(), cert_bytes.begin(), cert_bytes.end());
    } else {
        data.push_back(0);  // No rotation cert
    }
    
    // Signature (64 bytes)
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<KeyRevocation> KeyRevocation::from_bytes(const bytes& data) {
    if (data.size() < 32 + 1 + 8 + 32 + 1 + 1 + 64) {
        return std::nullopt;
    }
    
    KeyRevocation rev;
    size_t offset = 0;
    
    // Revoked key
    std::copy(data.begin(), data.begin() + 32, rev.revoked_key.begin());
    offset += 32;
    
    // Reason
    rev.reason = static_cast<KeyRevocationReason>(data[offset++]);
    
    // Revoked at
    rev.revoked_at = 0;
    for (int i = 0; i < 8; i++) {
        rev.revoked_at |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Revoker
    std::copy(data.begin() + offset, data.begin() + offset + 32, rev.revoker.id.begin());
    offset += 32;
    
    // Replacement key
    if (data[offset++] == 1) {
        if (data.size() < offset + 32) {
            return std::nullopt;
        }
        PublicKey replacement;
        std::copy(data.begin() + offset, data.begin() + offset + 32, replacement.begin());
        rev.replacement_key = replacement;
        offset += 32;
    }
    
    // Rotation certificate
    if (offset >= data.size()) {
        return std::nullopt;
    }
    if (data[offset++] == 1) {
        if (data.size() < offset + 4) {
            return std::nullopt;
        }
        
        uint32_t cert_len = 0;
        for (int i = 0; i < 4; i++) {
            cert_len |= static_cast<uint32_t>(data[offset++]) << (i * 8);
        }
        
        if (data.size() < offset + cert_len) {
            return std::nullopt;
        }
        
        // TODO: Parse RotationCertificate from bytes
        // For now, skip the cert data
        offset += cert_len;
    }
    
    // Signature
    if (data.size() < offset + 64) {
        return std::nullopt;
    }
    std::copy(data.begin() + offset, data.begin() + offset + 64, rev.signature.begin());
    
    return rev;
}

Hash256 KeyRevocation::get_id() const {
    bytes data;
    data.insert(data.end(), revoked_key.begin(), revoked_key.end());
    data.push_back(static_cast<uint8_t>(reason));
    
    uint64_t ts = revoked_at;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(ts >> (i * 8)));
    }
    
    return crypto::Blake3::hash(data);
}

// KeyRevocationList methods

bytes KeyRevocationList::to_bytes() const {
    bytes data;
    
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

std::optional<KeyRevocationList> KeyRevocationList::from_bytes(const bytes& data) {
    if (data.size() < 8 + 32 + 4 + 64) {
        return std::nullopt;
    }
    
    KeyRevocationList list;
    size_t offset = 0;
    
    // Timestamp
    list.timestamp = 0;
    for (int i = 0; i < 8; i++) {
        list.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Source node
    std::copy(data.begin() + offset, data.begin() + offset + 32, list.source_node.id.begin());
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
        
        bytes rev_data(data.begin() + offset, data.begin() + offset + len);
        auto rev = KeyRevocation::from_bytes(rev_data);
        if (!rev) {
            return std::nullopt;
        }
        
        list.revocations.push_back(*rev);
        offset += len;
    }
    
    // Signature
    if (data.size() < offset + 64) {
        return std::nullopt;
    }
    std::copy(data.begin() + offset, data.begin() + offset + 64, list.signature.begin());
    
    return list;
}

// KeyRevocationBroadcaster methods

KeyRevocationBroadcaster::KeyRevocationBroadcaster()
    : revocation_expiry_days_(365)  // Keys stay revoked for 1 year
    , max_propagation_count_(10)
{
    CASHEW_LOG_INFO("KeyRevocationBroadcaster initialized");
}

std::optional<KeyRevocation> KeyRevocationBroadcaster::revoke_key(
    const PublicKey& revoked_key,
    KeyRevocationReason reason,
    const NodeID& revoker,
    const std::optional<PublicKey>& replacement_key,
    const std::optional<core::RotationCertificate>& rotation_cert
) {
    // Check if already revoked
    if (is_key_revoked(revoked_key)) {
        CASHEW_LOG_WARN("Key already revoked");
        return std::nullopt;
    }
    
    KeyRevocation revocation;
    revocation.revoked_key = revoked_key;
    revocation.reason = reason;
    revocation.revoked_at = current_timestamp();
    revocation.revoker = revoker;
    revocation.replacement_key = replacement_key;
    revocation.rotation_cert = rotation_cert;
    // Note: Signature would be added by sign_revocation()
    
    // Store
    revocations_[revoked_key] = revocation;
    
    // Track replacement
    if (replacement_key) {
        key_replacements_[revoked_key] = *replacement_key;
    }
    
    CASHEW_LOG_INFO("Key revoked: reason={}", key_revocation_reason_to_string(reason));
    
    return revocation;
}

bool KeyRevocationBroadcaster::is_key_revoked(const PublicKey& public_key) const {
    return revocations_.count(public_key) > 0;
}

std::optional<KeyRevocation> KeyRevocationBroadcaster::get_revocation(const PublicKey& public_key) const {
    auto it = revocations_.find(public_key);
    if (it == revocations_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<PublicKey> KeyRevocationBroadcaster::get_replacement_key(const PublicKey& revoked_key) const {
    auto it = key_replacements_.find(revoked_key);
    if (it == key_replacements_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool KeyRevocationBroadcaster::process_revocation(
    const KeyRevocation& revocation,
    const NodeID& source_node
) {
    Hash256 id = revocation.get_id();
    
    // Check if already seen
    if (seen_revocations_.count(id) > 0) {
        return false;
    }
    
    // Validate
    if (!should_accept_revocation(revocation)) {
        CASHEW_LOG_WARN("Rejecting invalid revocation from source");
        return false;
    }
    
    // Verify signature
    if (!verify_revocation(revocation)) {
        CASHEW_LOG_WARN("Revocation signature verification failed");
        return false;
    }
    
    // Store
    revocations_[revocation.revoked_key] = revocation;
    seen_revocations_.insert(id);
    
    // Track replacement
    if (revocation.replacement_key) {
        key_replacements_[revocation.revoked_key] = *revocation.replacement_key;
    }
    
    CASHEW_LOG_INFO("Processed key revocation from network");
    
    return true;
}

size_t KeyRevocationBroadcaster::process_revocation_list(const KeyRevocationList& list) {
    size_t accepted = 0;
    
    for (const auto& revocation : list.revocations) {
        if (process_revocation(revocation, list.source_node)) {
            accepted++;
        }
    }
    
    CASHEW_LOG_INFO("Processed revocation list: {}/{} accepted",
                    accepted, list.revocations.size());
    
    return accepted;
}

KeyRevocationList KeyRevocationBroadcaster::create_revocation_list(bool include_all) const {
    KeyRevocationList list;
    list.timestamp = current_timestamp();
    // Note: source_node and signature filled by caller
    
    if (include_all) {
        for (const auto& [key, revocation] : revocations_) {
            list.revocations.push_back(revocation);
        }
    } else {
        // Only recent (last hour)
        uint64_t one_hour_ago = current_timestamp() - 3600;
        list.revocations = get_recent_revocations(one_hour_ago, 100);
    }
    
    return list;
}

std::vector<KeyRevocation> KeyRevocationBroadcaster::get_recent_revocations(
    uint64_t since_timestamp,
    size_t max_count
) const {
    std::vector<KeyRevocation> result;
    
    for (const auto& [key, revocation] : revocations_) {
        if (revocation.revoked_at >= since_timestamp) {
            result.push_back(revocation);
            
            if (result.size() >= max_count) {
                break;
            }
        }
    }
    
    return result;
}

bool KeyRevocationBroadcaster::verify_revocation(const KeyRevocation& revocation) {
    // Serialize data for verification (excluding signature)
    bytes data;
    
    data.insert(data.end(), revocation.revoked_key.begin(), revocation.revoked_key.end());
    data.push_back(static_cast<uint8_t>(revocation.reason));
    
    uint64_t ts = revocation.revoked_at;
    for (int i = 0; i < 8; i++) {
        data.push_back(static_cast<uint8_t>(ts >> (i * 8)));
    }
    
    data.insert(data.end(), revocation.revoker.id.begin(), revocation.revoker.id.end());
    
    if (revocation.replacement_key) {
        data.insert(data.end(), revocation.replacement_key->begin(), revocation.replacement_key->end());
    }
    
    // Verify with replacement key if present (key owner signed with new key)
    // Otherwise verify with revoked key (self-revocation) or revoker's key
    PublicKey verifying_key = revocation.replacement_key.value_or(revocation.revoked_key);
    
    return crypto::Ed25519::verify(data, revocation.signature, verifying_key);
}

void KeyRevocationBroadcaster::sign_revocation(KeyRevocation& revocation, const Signature& signature) {
    revocation.signature = signature;
}

size_t KeyRevocationBroadcaster::revocation_count() const {
    return revocations_.size();
}

size_t KeyRevocationBroadcaster::expired_revocation_count() const {
    size_t count = 0;
    for (const auto& [key, revocation] : revocations_) {
        if (is_revocation_expired(revocation)) {
            count++;
        }
    }
    return count;
}

void KeyRevocationBroadcaster::cleanup_expired_revocations() {
    std::vector<PublicKey> to_remove;
    
    for (const auto& [key, revocation] : revocations_) {
        if (is_revocation_expired(revocation)) {
            to_remove.push_back(key);
        }
    }
    
    for (const auto& key : to_remove) {
        revocations_.erase(key);
        key_replacements_.erase(key);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_INFO("Cleaned up {} expired key revocations", to_remove.size());
    }
}

// Private methods

bool KeyRevocationBroadcaster::is_revocation_expired(const KeyRevocation& revocation) const {
    uint64_t expiry_seconds = revocation_expiry_days_ * 24 * 3600;
    uint64_t expiry_time = revocation.revoked_at + expiry_seconds;
    return current_timestamp() >= expiry_time;
}

bool KeyRevocationBroadcaster::should_accept_revocation(const KeyRevocation& revocation) const {
    // Check if expired
    if (is_revocation_expired(revocation)) {
        return false;
    }
    
    // Check if from future (clock skew tolerance: 5 minutes)
    uint64_t now = current_timestamp();
    if (revocation.revoked_at > now + 300) {
        return false;
    }
    
    // If rotation certificate present, verify it
    if (revocation.rotation_cert) {
        if (!revocation.rotation_cert->verify()) {
            CASHEW_LOG_WARN("Rotation certificate verification failed");
            return false;
        }
    }
    
    return true;
}

uint64_t KeyRevocationBroadcaster::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::system_clock::to_time_t(now);
}

// Helper functions

const char* key_revocation_reason_to_string(KeyRevocationReason reason) {
    switch (reason) {
        case KeyRevocationReason::SUSPECTED_COMPROMISE: return "Suspected compromise";
        case KeyRevocationReason::CONFIRMED_COMPROMISE: return "Confirmed compromise";
        case KeyRevocationReason::SCHEDULED_ROTATION: return "Scheduled rotation";
        case KeyRevocationReason::DEVICE_LOSS: return "Device loss";
        case KeyRevocationReason::KEY_EXPIRATION: return "Key expiration";
        case KeyRevocationReason::POLICY_VIOLATION: return "Policy violation";
        case KeyRevocationReason::ADMINISTRATIVE: return "Administrative";
        case KeyRevocationReason::OWNER_REQUEST: return "Owner request";
        default: return "Unknown";
    }
}

} // namespace cashew::security
