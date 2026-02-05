#pragma once

#include "cashew/common.hpp"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace cashew::core {

/**
 * ThingType - Type of content a Thing represents
 */
enum class ThingType : uint8_t {
    UNKNOWN = 0,
    GAME = 1,           // Interactive game
    DICTIONARY = 2,     // Reference data
    DATASET = 3,        // Structured data
    APP = 4,            // Web application
    DOCUMENT = 5,       // Static document
    MEDIA = 6,          // Video/audio content
    LIBRARY = 7,        // Code library
    FORUM = 8           // Discussion board
};

/**
 * ThingMetadata - Metadata describing a Thing
 */
struct ThingMetadata {
    ContentHash content_hash;           // BLAKE3 hash of content
    std::string name;                   // Human-readable name
    std::string description;            // Description
    ThingType type;                     // Type of content
    size_t size_bytes;                  // Total size in bytes
    uint64_t created_timestamp;         // Creation time
    NodeID creator_id;                  // Creator's node ID
    std::vector<std::string> tags;      // Search tags
    uint32_t version;                   // Version number
    Signature creator_signature;        // Ed25519 signature
    
    // Optional fields
    std::string mime_type;              // MIME type
    std::string entry_point;            // Entry file (e.g., index.html)
    
    /**
     * Serialize metadata to bytes
     */
    bytes serialize() const;
    
    /**
     * Deserialize metadata from bytes
     */
    static std::optional<ThingMetadata> deserialize(const bytes& data);
    
    /**
     * Verify creator signature
     */
    bool verify_signature(const PublicKey& creator_public_key) const;
};

/**
 * Thing - A content item in the Cashew network
 * 
 * Things are content-addressed, immutable units with max 500MB size.
 * They can be games, dictionaries, datasets, apps, or any static content.
 */
class Thing {
public:
    /**
     * Maximum Thing size (500 MB)
     */
    static constexpr size_t MAX_SIZE = 500 * 1024 * 1024;
    
    /**
     * Create a new Thing from data
     * @param data Content data
     * @param metadata Thing metadata
     * @return Thing or nullopt if invalid
     */
    static std::optional<Thing> create(
        bytes data,
        const ThingMetadata& metadata
    );
    
    /**
     * Load a Thing from storage
     * @param content_hash Content hash
     * @return Thing or nullopt if not found
     */
    static std::optional<Thing> load(const ContentHash& content_hash);
    
    /**
     * Get content hash
     */
    const ContentHash& content_hash() const { return metadata_.content_hash; }
    
    /**
     * Get metadata
     */
    const ThingMetadata& metadata() const { return metadata_; }
    
    /**
     * Get content data
     */
    const bytes& data() const { return data_; }
    
    /**
     * Get content size
     */
    size_t size() const { return data_.size(); }
    
    /**
     * Verify content integrity (hash matches)
     */
    bool verify_integrity() const;
    
    /**
     * Save Thing to storage
     * @return True if successful
     */
    bool save() const;
    
    /**
     * Get a chunk of the content
     * @param offset Start offset
     * @param length Chunk length
     * @return Chunk data
     */
    bytes get_chunk(size_t offset, size_t length) const;

private:
    Thing(bytes data, ThingMetadata metadata)
        : data_(std::move(data))
        , metadata_(std::move(metadata))
    {}
    
    bytes data_;
    ThingMetadata metadata_;
};

} // namespace cashew::core
