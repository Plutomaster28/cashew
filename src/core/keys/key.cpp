#include "key.hpp"
#include "utils/logger.hpp"
#include <cstring>
#include <sstream>

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
