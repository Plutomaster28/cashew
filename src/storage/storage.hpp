#pragma once

#include "cashew/common.hpp"
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

namespace cashew::storage {

/**
 * Storage backend interface for content-addressed storage
 * Uses LevelDB for metadata and filesystem for content blobs
 */
class Storage {
public:
    /**
     * Initialize storage with a data directory
     * @param data_dir Directory for storage (creates if doesn't exist)
     */
    explicit Storage(const std::filesystem::path& data_dir);
    ~Storage();
    
    // Disable copy, allow move
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    Storage(Storage&&) noexcept;
    Storage& operator=(Storage&&) noexcept;
    
    /**
     * Store content with its hash as key
     * @param content_hash Content hash (key)
     * @param data Content data
     * @return True if successful
     */
    bool put_content(const ContentHash& content_hash, const bytes& data);
    
    /**
     * Retrieve content by hash
     * @param content_hash Content hash
     * @return Content data or nullopt if not found
     */
    std::optional<bytes> get_content(const ContentHash& content_hash) const;
    
    /**
     * Check if content exists
     * @param content_hash Content hash
     * @return True if exists
     */
    bool has_content(const ContentHash& content_hash) const;
    
    /**
     * Delete content
     * @param content_hash Content hash
     * @return True if successful
     */
    bool delete_content(const ContentHash& content_hash);
    
    /**
     * Store metadata (key-value)
     * @param key Metadata key
     * @param value Metadata value
     * @return True if successful
     */
    bool put_metadata(const std::string& key, const bytes& value);
    
    /**
     * Retrieve metadata
     * @param key Metadata key
     * @return Metadata value or nullopt if not found
     */
    std::optional<bytes> get_metadata(const std::string& key) const;
    
    /**
     * Delete metadata
     * @param key Metadata key
     * @return True if successful
     */
    bool delete_metadata(const std::string& key);
    
    /**
     * List all content hashes
     * @return Vector of content hashes
     */
    std::vector<ContentHash> list_content() const;
    
    /**
     * Get total storage size in bytes
     * @return Total size
     */
    size_t total_size() const;
    
    /**
     * Get number of stored items
     * @return Item count
     */
    size_t item_count() const;
    
    /**
     * Compact storage (remove fragmentation)
     */
    void compact();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cashew::storage
