#include "thing.hpp"
#include "../../crypto/blake3.hpp"
#include "../../crypto/ed25519.hpp"
#include "../../storage/storage.hpp"
#include "../../utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <cstdlib>

namespace cashew::core {

namespace {

template <typename T>
bool read_scalar(const bytes& data, size_t& offset, T& out) {
    if (offset + sizeof(T) > data.size()) {
        return false;
    }
    std::memcpy(&out, data.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

bool read_blob(const bytes& data, size_t& offset, uint8_t* out, size_t len) {
    if (offset + len > data.size()) {
        return false;
    }
    std::memcpy(out, data.data() + offset, len);
    offset += len;
    return true;
}

bool read_string(const bytes& data, size_t& offset, std::string& out) {
    uint32_t len = 0;
    if (!read_scalar(data, offset, len)) {
        return false;
    }
    if (offset + len > data.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(data.data() + offset), len);
    offset += len;
    return true;
}

std::filesystem::path default_data_dir() {
    if (const char* env_data_dir = std::getenv("CASHEW_DATA_DIR"); env_data_dir && env_data_dir[0] != '\0') {
        return std::filesystem::path(env_data_dir);
    }
    return std::filesystem::path("./data");
}

std::string metadata_key(const ContentHash& hash) {
    return "thing_meta_" + hash.to_string();
}

} // namespace

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
    // Compact binary serialization for Thing metadata persistence.
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

std::optional<ThingMetadata> ThingMetadata::deserialize(const bytes& data) {
    ThingMetadata metadata;
    size_t offset = 0;

    if (!read_blob(data, offset, metadata.content_hash.hash.data(), metadata.content_hash.hash.size())) {
        return std::nullopt;
    }

    if (!read_string(data, offset, metadata.name)) return std::nullopt;
    if (!read_string(data, offset, metadata.description)) return std::nullopt;

    uint8_t type_val = 0;
    if (!read_scalar(data, offset, type_val)) return std::nullopt;
    metadata.type = static_cast<ThingType>(type_val);

    if (!read_scalar(data, offset, metadata.size_bytes)) return std::nullopt;
    if (!read_scalar(data, offset, metadata.created_timestamp)) return std::nullopt;

    if (!read_blob(data, offset, metadata.creator_id.id.data(), metadata.creator_id.id.size())) {
        return std::nullopt;
    }

    uint32_t tag_count = 0;
    if (!read_scalar(data, offset, tag_count)) return std::nullopt;
    metadata.tags.clear();
    metadata.tags.reserve(tag_count);
    for (uint32_t i = 0; i < tag_count; ++i) {
        std::string tag;
        if (!read_string(data, offset, tag)) return std::nullopt;
        metadata.tags.push_back(std::move(tag));
    }

    if (!read_scalar(data, offset, metadata.version)) return std::nullopt;

    if (!read_blob(data, offset, metadata.creator_signature.data(), metadata.creator_signature.size())) {
        return std::nullopt;
    }

    if (!read_string(data, offset, metadata.mime_type)) return std::nullopt;
    if (!read_string(data, offset, metadata.entry_point)) return std::nullopt;

    return metadata;
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

std::optional<Thing> Thing::load(const ContentHash& content_hash) {
    storage::Storage storage(default_data_dir() / "storage");

    auto data_opt = storage.get_content(content_hash);
    if (!data_opt) {
        return std::nullopt;
    }

    auto meta_bytes = storage.get_metadata(metadata_key(content_hash));
    if (!meta_bytes) {
        CASHEW_LOG_WARN("Thing metadata missing for {}", content_hash.to_string());
        return std::nullopt;
    }

    auto metadata_opt = ThingMetadata::deserialize(*meta_bytes);
    if (!metadata_opt) {
        CASHEW_LOG_ERROR("Failed to deserialize Thing metadata for {}", content_hash.to_string());
        return std::nullopt;
    }

    return Thing::create(std::move(*data_opt), *metadata_opt);
}

bool Thing::verify_integrity() const {
    Hash256 computed = crypto::Blake3::hash(data_);
    ContentHash computed_hash(computed);
    return computed_hash == metadata_.content_hash;
}

bool Thing::save() const {
    storage::Storage storage(default_data_dir() / "storage");

    if (!storage.put_content(metadata_.content_hash, data_)) {
        return false;
    }

    const bytes metadata_bytes = metadata_.serialize();
    if (!storage.put_metadata(metadata_key(metadata_.content_hash), metadata_bytes)) {
        CASHEW_LOG_ERROR("Failed to save Thing metadata for {}", metadata_.content_hash.to_string());
        return false;
    }

    return true;
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
