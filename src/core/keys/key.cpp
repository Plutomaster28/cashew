#include "key.hpp"
#include "crypto/ed25519.hpp"
#include "crypto/blake3.hpp"
#include "utils/logger.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>

namespace cashew::core {

Key Key::create(
    KeyType type,
    const NodeID& owner_id,
    uint64_t issued_timestamp,
    const std::string& source
) {
    return Key(type, owner_id, issued_timestamp, issued_timestamp, source);
}

void Key::mark_used(uint64_t current_time) {
    last_used_timestamp_ = current_time;
    CASHEW_LOG_DEBUG("Key {} used at {}", 
                    key_type_to_string(type_), current_time);
}

bool Key::has_decayed(uint64_t current_time) const {
    uint64_t age = current_time - last_used_timestamp_;
    return age >= DECAY_PERIOD_SECONDS;
}

uint64_t Key::time_until_decay(uint64_t current_time) const {
    if (has_decayed(current_time)) {
        return 0;
    }
    
    uint64_t age = current_time - last_used_timestamp_;
    return DECAY_PERIOD_SECONDS - age;
}

bytes Key::serialize() const {
    // Simple serialization (TODO: use MessagePack)
    std::ostringstream oss;
    
    uint8_t type_val = static_cast<uint8_t>(type_);
    oss.write(reinterpret_cast<const char*>(&type_val), sizeof(type_val));
    oss.write(reinterpret_cast<const char*>(owner_id_.id.data()), owner_id_.id.size());
    oss.write(reinterpret_cast<const char*>(&issued_timestamp_), sizeof(issued_timestamp_));
    oss.write(reinterpret_cast<const char*>(&last_used_timestamp_), sizeof(last_used_timestamp_));
    
    uint32_t source_len = static_cast<uint32_t>(source_.size());
    oss.write(reinterpret_cast<const char*>(&source_len), sizeof(source_len));
    oss.write(source_.data(), source_.size());
    
    std::string str = oss.str();
    return bytes(str.begin(), str.end());
}

std::optional<Key> Key::deserialize(const bytes& /* data */) {
    // TODO: Implement proper deserialization
    CASHEW_LOG_WARN("Key deserialization not implemented yet");
    return std::nullopt;
}

std::string Key::get_key_id() const {
    // Generate unique key ID from type, owner, and timestamp
    std::ostringstream oss;
    oss << key_type_to_string(type_) << "_"
        << crypto::Blake3::hash_to_hex(owner_id_.id) << "_"
        << issued_timestamp_;
    
    // Hash to get fixed-length ID
    auto hash = crypto::Blake3::hash(bytes(oss.str().begin(), oss.str().end()));
    return crypto::Blake3::hash_to_hex(hash);
}

// ============================================================================
// KeyTransfer Implementation
// ============================================================================

bytes KeyTransfer::to_bytes() const {
    std::ostringstream oss;
    
    // Serialize fields
    uint32_t key_id_len = static_cast<uint32_t>(key_id.size());
    oss.write(reinterpret_cast<const char*>(&key_id_len), sizeof(key_id_len));
    oss.write(key_id.data(), key_id.size());
    
    oss.write(reinterpret_cast<const char*>(from_node.id.data()), from_node.id.size());
    oss.write(reinterpret_cast<const char*>(to_node.id.data()), to_node.id.size());
    
    uint8_t type_val = static_cast<uint8_t>(key_type);
    oss.write(reinterpret_cast<const char*>(&type_val), sizeof(type_val));
    oss.write(reinterpret_cast<const char*>(&transfer_timestamp), sizeof(transfer_timestamp));
    
    uint32_t reason_len = static_cast<uint32_t>(reason.size());
    oss.write(reinterpret_cast<const char*>(&reason_len), sizeof(reason_len));
    oss.write(reason.data(), reason.size());
    
    oss.write(reinterpret_cast<const char*>(from_signature.data()), from_signature.size());
    
    std::string str = oss.str();
    return bytes(str.begin(), str.end());
}

std::optional<KeyTransfer> KeyTransfer::from_bytes(const bytes& data) {
    // TODO: Implement deserialization
    return std::nullopt;
}

bool KeyTransfer::verify_signature(const PublicKey& from_public_key) const {
    // Create message to verify (everything except signature)
    std::ostringstream oss;
    
    uint32_t key_id_len = static_cast<uint32_t>(key_id.size());
    oss.write(reinterpret_cast<const char*>(&key_id_len), sizeof(key_id_len));
    oss.write(key_id.data(), key_id.size());
    
    oss.write(reinterpret_cast<const char*>(from_node.id.data()), from_node.id.size());
    oss.write(reinterpret_cast<const char*>(to_node.id.data()), to_node.id.size());
    
    uint8_t type_val = static_cast<uint8_t>(key_type);
    oss.write(reinterpret_cast<const char*>(&type_val), sizeof(type_val));
    oss.write(reinterpret_cast<const char*>(&transfer_timestamp), sizeof(transfer_timestamp));
    
    uint32_t reason_len = static_cast<uint32_t>(reason.size());
    oss.write(reinterpret_cast<const char*>(&reason_len), sizeof(reason_len));
    oss.write(reason.data(), reason.size());
    
    std::string str = oss.str();
    bytes message(str.begin(), str.end());
    
    return crypto::Ed25519::verify(message, from_signature, from_public_key);
}

// ============================================================================
// KeyVouch Implementation
// ============================================================================

bytes KeyVouch::to_bytes() const {
    std::ostringstream oss;
    
    oss.write(reinterpret_cast<const char*>(voucher.id.data()), voucher.id.size());
    oss.write(reinterpret_cast<const char*>(vouchee.id.data()), vouchee.id.size());
    
    uint8_t type_val = static_cast<uint8_t>(key_type);
    oss.write(reinterpret_cast<const char*>(&type_val), sizeof(type_val));
    oss.write(reinterpret_cast<const char*>(&key_count), sizeof(key_count));
    oss.write(reinterpret_cast<const char*>(&vouch_timestamp), sizeof(vouch_timestamp));
    
    uint32_t statement_len = static_cast<uint32_t>(statement.size());
    oss.write(reinterpret_cast<const char*>(&statement_len), sizeof(statement_len));
    oss.write(statement.data(), statement.size());
    
    oss.write(reinterpret_cast<const char*>(voucher_signature.data()), voucher_signature.size());
    
    std::string str = oss.str();
    return bytes(str.begin(), str.end());
}

std::optional<KeyVouch> KeyVouch::from_bytes(const bytes& data) {
    // TODO: Implement deserialization
    return std::nullopt;
}

bool KeyVouch::verify_signature(const PublicKey& voucher_public_key) const {
    // Create message to verify (everything except signature)
    std::ostringstream oss;
    
    oss.write(reinterpret_cast<const char*>(voucher.id.data()), voucher.id.size());
    oss.write(reinterpret_cast<const char*>(vouchee.id.data()), vouchee.id.size());
    
    uint8_t type_val = static_cast<uint8_t>(key_type);
    oss.write(reinterpret_cast<const char*>(&type_val), sizeof(type_val));
    oss.write(reinterpret_cast<const char*>(&key_count), sizeof(key_count));
    oss.write(reinterpret_cast<const char*>(&vouch_timestamp), sizeof(vouch_timestamp));
    
    uint32_t statement_len = static_cast<uint32_t>(statement.size());
    oss.write(reinterpret_cast<const char*>(&statement_len), sizeof(statement_len));
    oss.write(statement.data(), statement.size());
    
    std::string str = oss.str();
    bytes message(str.begin(), str.end());
    
    return crypto::Ed25519::verify(message, voucher_signature, voucher_public_key);
}

// ============================================================================
// KeyManager Implementation
// ============================================================================

void KeyManager::add_key(const Key& key) {
    std::string key_id = key.get_key_id();
    keys_.insert({key_id, key});
    key_index_[key.owner()].push_back(key_id);
    
    CASHEW_LOG_INFO("Added key {} of type {} for node {}",
                   key_id.substr(0, 8), key_type_to_string(key.type()),
                   crypto::Blake3::hash_to_hex(key.owner().id).substr(0, 8));
}

bool KeyManager::remove_key(const std::string& key_id) {
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        return false;
    }
    
    // Remove from index
    NodeID owner = it->second.owner();
    auto& owner_keys = key_index_[owner];
    owner_keys.erase(std::remove(owner_keys.begin(), owner_keys.end(), key_id), 
                     owner_keys.end());
    
    keys_.erase(it);
    CASHEW_LOG_INFO("Removed key {}", key_id.substr(0, 8));
    return true;
}

std::optional<Key> KeyManager::get_key(const std::string& key_id) const {
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Key> KeyManager::get_keys_by_type(KeyType type) const {
    std::vector<Key> result;
    for (const auto& [_, key] : keys_) {
        if (key.type() == type) {
            result.push_back(key);
        }
    }
    return result;
}

std::vector<Key> KeyManager::get_keys_by_owner(const NodeID& owner) const {
    std::vector<Key> result;
    auto it = key_index_.find(owner);
    if (it != key_index_.end()) {
        for (const auto& key_id : it->second) {
            auto key = get_key(key_id);
            if (key) {
                result.push_back(*key);
            }
        }
    }
    return result;
}

uint32_t KeyManager::count_keys(const NodeID& owner, KeyType type) const {
    uint32_t count = 0;
    auto keys = get_keys_by_owner(owner);
    for (const auto& key : keys) {
        if (key.type() == type) {
            count++;
        }
    }
    return count;
}

bool KeyManager::can_transfer(const NodeID& from, const NodeID& to, KeyType type) const {
    // Must have at least MIN_KEYS_TO_TRANSFER keys of this type to transfer one
    uint32_t key_count = count_keys(from, type);
    return key_count >= MIN_KEYS_TO_TRANSFER;
}

std::optional<KeyTransfer> KeyManager::create_transfer(
    const std::string& key_id,
    const NodeID& from,
    const NodeID& to,
    const std::string& reason,
    const SecretKey& from_secret_key
) {
    // Verify key exists and belongs to sender
    auto key = get_key(key_id);
    if (!key || key->owner() != from) {
        CASHEW_LOG_WARN("Transfer failed: key {} not found or not owned by sender", 
                       key_id.substr(0, 8));
        return std::nullopt;
    }
    
    // Check if sender can transfer
    if (!can_transfer(from, to, key->type())) {
        CASHEW_LOG_WARN("Transfer failed: sender doesn't have enough keys");
        return std::nullopt;
    }
    
    // Create transfer record
    KeyTransfer transfer;
    transfer.key_id = key_id;
    transfer.from_node = from;
    transfer.to_node = to;
    transfer.key_type = key->type();
    transfer.transfer_timestamp = std::time(nullptr);
    transfer.reason = reason;
    
    // Sign the transfer
    bytes message_bytes = transfer.to_bytes();
    // Remove signature bytes for signing (signature field is at end)
    message_bytes.resize(message_bytes.size() - transfer.from_signature.size());
    
    transfer.from_signature = crypto::Ed25519::sign(message_bytes, from_secret_key);
    
    return transfer;
}

bool KeyManager::execute_transfer(const KeyTransfer& transfer) {
    // Verify key exists
    auto key = get_key(transfer.key_id);
    if (!key) {
        CASHEW_LOG_ERROR("Transfer failed: key not found");
        return false;
    }
    
    // Verify ownership
    if (key->owner() != transfer.from_node) {
        CASHEW_LOG_ERROR("Transfer failed: key not owned by sender");
        return false;
    }
    
    // Remove old key
    if (!remove_key(transfer.key_id)) {
        CASHEW_LOG_ERROR("Transfer failed: couldn't remove old key");
        return false;
    }
    
    // Create new key for recipient
    Key new_key = Key::create(
        transfer.key_type,
        transfer.to_node,
        transfer.transfer_timestamp,
        "transferred"
    );
    
    add_key(new_key);
    transfer_history_.push_back(transfer);
    
    CASHEW_LOG_INFO("Transferred key {} from {} to {}",
                   transfer.key_id.substr(0, 8),
                   crypto::Blake3::hash_to_hex(transfer.from_node.id).substr(0, 8),
                   crypto::Blake3::hash_to_hex(transfer.to_node.id).substr(0, 8));
    
    return true;
}

std::vector<KeyTransfer> KeyManager::get_transfer_history(const NodeID& node) const {
    std::vector<KeyTransfer> result;
    for (const auto& transfer : transfer_history_) {
        if (transfer.from_node == node || transfer.to_node == node) {
            result.push_back(transfer);
        }
    }
    return result;
}

bool KeyManager::can_vouch(const NodeID& voucher, const NodeID& vouchee, KeyType type) const {
    // Must have keys to vouch
    uint32_t voucher_keys = count_keys(voucher, type);
    if (voucher_keys == 0) {
        return false;
    }
    
    // Check epoch limit
    auto it = vouch_counts_this_epoch_.find(voucher);
    if (it != vouch_counts_this_epoch_.end() && it->second >= MAX_VOUCH_PER_EPOCH) {
        return false;
    }
    
    return true;
}

std::optional<KeyVouch> KeyManager::create_vouch(
    const NodeID& voucher,
    const NodeID& vouchee,
    KeyType key_type,
    uint32_t key_count,
    const std::string& statement,
    const SecretKey& voucher_secret_key
) {
    if (!can_vouch(voucher, vouchee, key_type)) {
        CASHEW_LOG_WARN("Vouch failed: voucher cannot vouch");
        return std::nullopt;
    }
    
    KeyVouch vouch;
    vouch.voucher = voucher;
    vouch.vouchee = vouchee;
    vouch.key_type = key_type;
    vouch.key_count = key_count;
    vouch.vouch_timestamp = std::time(nullptr);
    vouch.statement = statement;
    
    // Sign the vouch
    bytes message_bytes = vouch.to_bytes();
    // Remove signature bytes for signing
    message_bytes.resize(message_bytes.size() - vouch.voucher_signature.size());
    
    vouch.voucher_signature = crypto::Ed25519::sign(message_bytes, voucher_secret_key);
    
    return vouch;
}

bool KeyManager::execute_vouch(const KeyVouch& vouch, uint64_t current_time) {
    // Issue keys to vouchee
    for (uint32_t i = 0; i < vouch.key_count; ++i) {
        Key new_key = Key::create(
            vouch.key_type,
            vouch.vouchee,
            current_time,
            "vouched"
        );
        add_key(new_key);
    }
    
    vouch_records_.push_back(vouch);
    vouch_counts_this_epoch_[vouch.voucher]++;
    
    CASHEW_LOG_INFO("Vouching: {} vouched for {} to receive {} {} keys",
                   crypto::Blake3::hash_to_hex(vouch.voucher.id).substr(0, 8),
                   crypto::Blake3::hash_to_hex(vouch.vouchee.id).substr(0, 8),
                   vouch.key_count,
                   key_type_to_string(vouch.key_type));
    
    return true;
}

std::vector<KeyVouch> KeyManager::get_vouches_by(const NodeID& voucher) const {
    std::vector<KeyVouch> result;
    for (const auto& vouch : vouch_records_) {
        if (vouch.voucher == voucher) {
            result.push_back(vouch);
        }
    }
    return result;
}

std::vector<KeyVouch> KeyManager::get_vouches_for(const NodeID& vouchee) const {
    std::vector<KeyVouch> result;
    for (const auto& vouch : vouch_records_) {
        if (vouch.vouchee == vouchee) {
            result.push_back(vouch);
        }
    }
    return result;
}

KeyManager::VouchStats KeyManager::get_vouch_stats(const NodeID& node) const {
    VouchStats stats{};
    
    for (const auto& vouch : vouch_records_) {
        if (vouch.voucher == node) {
            stats.total_vouches_given++;
            stats.successful_vouches++;  // TODO: Track failures
        }
        if (vouch.vouchee == node) {
            stats.total_vouches_received++;
        }
    }
    
    return stats;
}

// ============================================================================
// KeyStore Implementation
// ============================================================================

KeyStore::KeyStore(const std::string& storage_path)
    : storage_path_(storage_path)
{
    // Create storage directories
    std::filesystem::create_directories(storage_path_ + "/keys");
    std::filesystem::create_directories(storage_path_ + "/transfers");
    std::filesystem::create_directories(storage_path_ + "/vouches");
}

bool KeyStore::save_key(const Key& key) {
    std::string key_id = key.get_key_id();
    std::string path = get_key_path(key_id);
    
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        CASHEW_LOG_ERROR("Failed to open key file for writing: {}", path);
        return false;
    }
    
    bytes data = key.serialize();
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    
    return file.good();
}

std::optional<Key> KeyStore::load_key(const std::string& key_id) {
    std::string path = get_key_path(key_id);
    
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    
    bytes data((std::istreambuf_iterator<char>(file)),
               std::istreambuf_iterator<char>());
    
    return Key::deserialize(data);
}

bool KeyStore::delete_key(const std::string& key_id) {
    std::string path = get_key_path(key_id);
    return std::filesystem::remove(path);
}

std::vector<Key> KeyStore::load_all_keys() {
    std::vector<Key> keys;
    std::string keys_dir = storage_path_ + "/keys";
    
    for (const auto& entry : std::filesystem::directory_iterator(keys_dir)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            if (file) {
                bytes data((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
                
                auto key = Key::deserialize(data);
                if (key) {
                    keys.push_back(*key);
                }
            }
        }
    }
    
    return keys;
}

bool KeyStore::save_transfer(const KeyTransfer& transfer) {
    std::string transfer_id = transfer.key_id + "_" + 
                             std::to_string(transfer.transfer_timestamp);
    std::string path = get_transfer_path(transfer_id);
    
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        CASHEW_LOG_ERROR("Failed to open transfer file for writing: {}", path);
        return false;
    }
    
    bytes data = transfer.to_bytes();
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    
    return file.good();
}

bool KeyStore::save_vouch(const KeyVouch& vouch) {
    std::string vouch_id = crypto::Blake3::hash_to_hex(vouch.vouchee.id) + "_" +
                          std::to_string(vouch.vouch_timestamp);
    std::string path = get_vouch_path(vouch_id);
    
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        CASHEW_LOG_ERROR("Failed to open vouch file for writing: {}", path);
        return false;
    }
    
    bytes data = vouch.to_bytes();
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    
    return file.good();
}

std::vector<KeyTransfer> KeyStore::load_transfers() {
    std::vector<KeyTransfer> transfers;
    std::string transfers_dir = storage_path_ + "/transfers";
    
    for (const auto& entry : std::filesystem::directory_iterator(transfers_dir)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            if (file) {
                bytes data((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
                
                auto transfer = KeyTransfer::from_bytes(data);
                if (transfer) {
                    transfers.push_back(*transfer);
                }
            }
        }
    }
    
    return transfers;
}

std::vector<KeyVouch> KeyStore::load_vouches() {
    std::vector<KeyVouch> vouches;
    std::string vouches_dir = storage_path_ + "/vouches";
    
    for (const auto& entry : std::filesystem::directory_iterator(vouches_dir)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            if (file) {
                bytes data((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
                
                auto vouch = KeyVouch::from_bytes(data);
                if (vouch) {
                    vouches.push_back(*vouch);
                }
            }
        }
    }
    
    return vouches;
}

bool KeyStore::clear_all() {
    bool success = true;
    
    success &= std::filesystem::remove_all(storage_path_ + "/keys") > 0;
    success &= std::filesystem::remove_all(storage_path_ + "/transfers") > 0;
    success &= std::filesystem::remove_all(storage_path_ + "/vouches") > 0;
    
    // Recreate directories
    std::filesystem::create_directories(storage_path_ + "/keys");
    std::filesystem::create_directories(storage_path_ + "/transfers");
    std::filesystem::create_directories(storage_path_ + "/vouches");
    
    return success;
}

std::string KeyStore::get_key_path(const std::string& key_id) const {
    return storage_path_ + "/keys/" + key_id + ".key";
}

std::string KeyStore::get_transfer_path(const std::string& transfer_id) const {
    return storage_path_ + "/transfers/" + transfer_id + ".transfer";
}

std::string KeyStore::get_vouch_path(const std::string& vouch_id) const {
    return storage_path_ + "/vouches/" + vouch_id + ".vouch";
}

std::string key_type_to_string(KeyType type) {
    switch (type) {
        case KeyType::IDENTITY: return "identity";
        case KeyType::NODE: return "node";
        case KeyType::NETWORK: return "network";
        case KeyType::SERVICE: return "service";
        case KeyType::ROUTING: return "routing";
        default: return "unknown";
    }
}

std::optional<KeyType> key_type_from_string(const std::string& str) {
    if (str == "identity") return KeyType::IDENTITY;
    if (str == "node") return KeyType::NODE;
    if (str == "network") return KeyType::NETWORK;
    if (str == "service") return KeyType::SERVICE;
    if (str == "routing") return KeyType::ROUTING;
    return std::nullopt;
}

} // namespace cashew::core
