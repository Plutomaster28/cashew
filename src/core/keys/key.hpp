#pragma once

#include "cashew/common.hpp"
#include <cstdint>
#include <string>
#include <optional>

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
    std::string source_;  // "pow", "postake", "vouched"
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
