#include "thing.hpp"
#include "../../crypto/blake3.hpp"
#include "../../crypto/ed25519.hpp"
#include "../../utils/logger.hpp"
#include <sstream>
#include <cstring>

namespace cashew::core {

// Helper to convert ThingType to string
static std::string thing_type_to_string(ThingType type) {
    switch (type) {
        case ThingType::GAME: return "game";
        case ThingType::DICTIONARY: return "dictionary";
        case ThingType::DATASET: return "dataset";
        case ThingType::APP: return "app";
        case ThingType::DOCUMENT: return "document";
        case ThingType::MEDIA: return "media";
        case ThingType::LIBRARY: return "library";
        case ThingType::FORUM: return "forum";
        default: return "unknown";
    }
}

bytes ThingMetadata::serialize() const {
    // Simple serialization (TODO: use MessagePack or Protobuf)
    std::ostringstream oss;
    
    // Write fields in order
    oss.write(reinterpret_cast<const char*>(content_hash.hash.data()), content_hash.hash.size());
    
    // Write string length + data
    auto write_string = [&](const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        oss.write(reinterpret_cast<const char*>(&len), sizeof(len));
        oss.write(str.data(), str.size());
    };
    
    write_string(name);
    write_string(description);
    
    uint8_t type_val = static_cast<uint8_t>(type);
    oss.write(reinterpret_cast<const char*>(&type_val), sizeof(type_val));
    oss.write(reinterpret_cast<const char*>(&size_bytes), sizeof(size_bytes));
    oss.write(reinterpret_cast<const char*>(&created_timestamp), sizeof(created_timestamp));
    oss.write(reinterpret_cast<const char*>(creator_id.id.data()), creator_id.id.size());
    
    // Write tags
    uint32_t tag_count = static_cast<uint32_t>(tags.size());
    oss.write(reinterpret_cast<const char*>(&tag_count), sizeof(tag_count));
    for (const auto& tag : tags) {
        write_string(tag);
    }
    
    oss.write(reinterpret_cast<const char*>(&version), sizeof(version));
    oss.write(reinterpret_cast<const char*>(creator_signature.data()), creator_signature.size());
    
    write_string(mime_type);
    write_string(entry_point);
    
    std::string str = oss.str();
    return bytes(str.begin(), str.end());
}

std::optional<ThingMetadata> ThingMetadata::deserialize(const bytes& /* data */) {
    // TODO: Implement proper deserialization
    CASHEW_LOG_WARN("ThingMetadata deserialization not fully implemented yet");
    return std::nullopt;
}

bool ThingMetadata::verify_signature(const PublicKey& creator_public_key) const {
    // Create message to verify (content_hash + name + timestamp)
    bytes message;
    message.insert(message.end(), content_hash.hash.begin(), content_hash.hash.end());
    message.insert(message.end(), name.begin(), name.end());
    
    bytes ts_bytes(sizeof(created_timestamp));
    std::memcpy(ts_bytes.data(), &created_timestamp, sizeof(created_timestamp));
    message.insert(message.end(), ts_bytes.begin(), ts_bytes.end());
    
    return crypto::Ed25519::verify(message, creator_signature, creator_public_key);
}

std::optional<Thing> Thing::create(
    bytes data,
    const ThingMetadata& metadata
) {
    // Validate size
    if (data.size() > MAX_SIZE) {
        CASHEW_LOG_ERROR("Thing size {} exceeds maximum {}", data.size(), MAX_SIZE);
        return std::nullopt;
    }
    
    if (data.empty()) {
        CASHEW_LOG_ERROR("Thing data is empty");
        return std::nullopt;
    }
    
    // Compute content hash
    Hash256 computed_hash = crypto::Blake3::hash(data);
    ContentHash content_hash_obj(computed_hash);
    
    // Verify hash matches metadata
    if (content_hash_obj != metadata.content_hash) {
        CASHEW_LOG_ERROR("Content hash mismatch");
        return std::nullopt;
    }
    
    // Verify size matches
    if (data.size() != metadata.size_bytes) {
        CASHEW_LOG_ERROR("Size mismatch: data={}, metadata={}", 
                        data.size(), metadata.size_bytes);
        return std::nullopt;
    }
    
    CASHEW_LOG_INFO("Created Thing: name='{}', type={}, size={}", 
                   metadata.name, thing_type_to_string(metadata.type), data.size());
    
    return Thing(std::move(data), metadata);
}

std::optional<Thing> Thing::load(const ContentHash& /* content_hash */) {
    // TODO: Implement storage backend loading
    CASHEW_LOG_WARN("Thing::load not implemented yet (storage backend needed)");
    return std::nullopt;
}

bool Thing::verify_integrity() const {
    Hash256 computed = crypto::Blake3::hash(data_);
    ContentHash computed_hash(computed);
    return computed_hash == metadata_.content_hash;
}

bool Thing::save() const {
    // TODO: Implement storage backend saving
    CASHEW_LOG_WARN("Thing::save not implemented yet (storage backend needed)");
    return false;
}

bytes Thing::get_chunk(size_t offset, size_t length) const {
    if (offset >= data_.size()) {
        return bytes();
    }
    
    size_t actual_length = std::min(length, data_.size() - offset);
    bytes chunk(data_.begin() + offset, data_.begin() + offset + actual_length);
    return chunk;
}

} // namespace cashew::core
