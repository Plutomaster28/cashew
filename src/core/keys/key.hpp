#pragma once

#include "cashew/common.hpp"
#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <map>

namespace cashew::core {

/**
 * KeyType - Different types of participation keys
 */
enum class KeyType : uint8_t {
    IDENTITY = 1,    // Create/manage identities
    NODE = 2,        // Run a node, participate in network
    NETWORK = 3,     // Form/join Networks (clusters)
    SERVICE = 4,     // Host Things (content)
    ROUTING = 5      // Route traffic, relay messages
};

/**
 * Key - Participation right (NOT currency)
 * 
 * Keys grant specific capabilities in the network.
 * They are earned via PoW, PoStake, or social vouching.
 * Keys decay over time if unused.
 */
class Key {
public:
    /**
     * Key decay period (30 days)
     */
    static constexpr uint64_t DECAY_PERIOD_SECONDS = 86400 * 30;
    
    /**
     * Create a new key
     * @param type Key type
     * @param owner_id Node that owns this key
     * @param issued_timestamp When key was issued
     * @param source How key was obtained (pow/postake/vouched)
     * @return Key
     */
    static Key create(
        KeyType type,
        const NodeID& owner_id,
        uint64_t issued_timestamp,
        const std::string& source
    );
    
    /**
     * Get key type
     */
    KeyType type() const { return type_; }
    
    /**
     * Get owner node ID
     */
    const NodeID& owner() const { return owner_id_; }
    
    /**
     * Get issuance timestamp
     */
    uint64_t issued_at() const { return issued_timestamp_; }
    
    /**
     * Get last used timestamp
     */
    uint64_t last_used() const { return last_used_timestamp_; }
    
    /**
     * Get source of key (pow/postake/vouched)
     */
    const std::string& source() const { return source_; }
    
    /**
     * Mark key as used (updates last_used timestamp)
     * @param current_time Current timestamp
     */
    void mark_used(uint64_t current_time);
    
    /**
     * Check if key has decayed
     * @param current_time Current timestamp
     * @return True if decayed
     */
    bool has_decayed(uint64_t current_time) const;
    
    /**
     * Get time until decay (seconds)
     * @param current_time Current timestamp
     * @return Seconds until decay (0 if already decayed)
     */
    uint64_t time_until_decay(uint64_t current_time) const;
    
    /**
     * Serialize key to bytes
     */
    bytes serialize() const;
    
    /**
     * Deserialize key from bytes
     */
    static std::optional<Key> deserialize(const bytes& data);
    
    /**
     * Get unique key ID (derived from type, owner, and timestamp)
     */
    std::string get_key_id() const;

private:
    Key(KeyType type, NodeID owner_id, uint64_t issued, 
        uint64_t last_used, std::string source)
        : type_(type)
        , owner_id_(std::move(owner_id))
        , issued_timestamp_(issued)
        , last_used_timestamp_(last_used)
        , source_(std::move(source))
    {}
    
    KeyType type_;
    NodeID owner_id_;
    uint64_t issued_timestamp_;
    uint64_t last_used_timestamp_;
    std::string source_;  // "pow", "postake", "vouched", "transferred"
};

/**
 * KeyTransfer - Record of key transfer between nodes
 */
struct KeyTransfer {
    std::string key_id;
    NodeID from_node;
    NodeID to_node;
    KeyType key_type;
    uint64_t transfer_timestamp;
    std::string reason;
    Signature from_signature;  // Signature from sender authorizing transfer
    
    bytes to_bytes() const;
    static std::optional<KeyTransfer> from_bytes(const bytes& data);
    bool verify_signature(const PublicKey& from_public_key) const;
};

/**
 * KeyVouch - Vouching record for issuing keys to new nodes
 */
struct KeyVouch {
    NodeID voucher;           // Established node vouching
    NodeID vouchee;           // New node being vouched for
    KeyType key_type;         // Type of key being vouched for
    uint32_t key_count;       // Number of keys vouched for (usually 1)
    uint64_t vouch_timestamp;
    std::string statement;    // Why vouching (optional)
    Signature voucher_signature;
    
    bytes to_bytes() const;
    static std::optional<KeyVouch> from_bytes(const bytes& data);
    bool verify_signature(const PublicKey& voucher_public_key) const;
};

/**
 * KeyManager - Manages key issuance, transfers, and vouching
 */
class KeyManager {
public:
    KeyManager() = default;
    ~KeyManager() = default;
    
    // Key inventory management
    void add_key(const Key& key);
    bool remove_key(const std::string& key_id);
    std::optional<Key> get_key(const std::string& key_id) const;
    std::vector<Key> get_keys_by_type(KeyType type) const;
    std::vector<Key> get_keys_by_owner(const NodeID& owner) const;
    uint32_t count_keys(const NodeID& owner, KeyType type) const;
    
    // Transfer system
    bool can_transfer(const NodeID& from, const NodeID& to, KeyType type) const;
    std::optional<KeyTransfer> create_transfer(
        const std::string& key_id,
        const NodeID& from,
        const NodeID& to,
        const std::string& reason,
        const SecretKey& from_secret_key
    );
    bool execute_transfer(const KeyTransfer& transfer);
    std::vector<KeyTransfer> get_transfer_history(const NodeID& node) const;
    
    // Vouching system
    bool can_vouch(const NodeID& voucher, const NodeID& vouchee, KeyType type) const;
    std::optional<KeyVouch> create_vouch(
        const NodeID& voucher,
        const NodeID& vouchee,
        KeyType key_type,
        uint32_t key_count,
        const std::string& statement,
        const SecretKey& voucher_secret_key
    );
    bool execute_vouch(const KeyVouch& vouch, uint64_t current_time);
    std::vector<KeyVouch> get_vouches_by(const NodeID& voucher) const;
    std::vector<KeyVouch> get_vouches_for(const NodeID& vouchee) const;
    
    // Requirements and limits
    static constexpr uint32_t MIN_REPUTATION_TO_VOUCH = 100;
    static constexpr uint32_t MIN_KEYS_TO_TRANSFER = 2;  // Must have at least 2 to transfer 1
    static constexpr uint32_t MAX_VOUCH_PER_EPOCH = 3;   // Can vouch for max 3 nodes per epoch
    
    // Statistics
    struct VouchStats {
        uint32_t total_vouches_given;
        uint32_t total_vouches_received;
        uint32_t successful_vouches;
        uint32_t failed_vouches;
    };
    
    VouchStats get_vouch_stats(const NodeID& node) const;
    
private:
    std::map<std::string, Key> keys_;                    // key_id -> Key
    std::map<NodeID, std::vector<std::string>> key_index_;  // owner -> key_ids
    std::vector<KeyTransfer> transfer_history_;
    std::vector<KeyVouch> vouch_records_;
    std::map<NodeID, uint32_t> vouch_counts_this_epoch_;  // Track per-epoch vouching limits
};

/**
 * KeyStore - Persistent storage for keys
 */
class KeyStore {
public:
    explicit KeyStore(const std::string& storage_path);
    ~KeyStore() = default;
    
    // Key persistence
    bool save_key(const Key& key);
    std::optional<Key> load_key(const std::string& key_id);
    bool delete_key(const std::string& key_id);
    std::vector<Key> load_all_keys();
    
    // Transfer/vouch persistence
    bool save_transfer(const KeyTransfer& transfer);
    bool save_vouch(const KeyVouch& vouch);
    std::vector<KeyTransfer> load_transfers();
    std::vector<KeyVouch> load_vouches();
    
    // Cleanup
    bool clear_all();
    
private:
    std::string storage_path_;
    std::string get_key_path(const std::string& key_id) const;
    std::string get_transfer_path(const std::string& transfer_id) const;
    std::string get_vouch_path(const std::string& vouch_id) const;
};

/**
 * Convert KeyType to string
 */
std::string key_type_to_string(KeyType type);

/**
 * Parse KeyType from string
 */
std::optional<KeyType> key_type_from_string(const std::string& str);

} // namespace cashew::core
