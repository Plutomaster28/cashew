#pragma once

#include "cashew/common.hpp"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>
#include <functional>

namespace cashew {
namespace gateway {

/**
 * Content types supported for rendering
 */
enum class ContentType {
    HTML,
    JAVASCRIPT,
    CSS,
    IMAGE_PNG,
    IMAGE_JPEG,
    IMAGE_GIF,
    IMAGE_WEBP,
    VIDEO_MP4,
    VIDEO_WEBM,
    AUDIO_MP3,
    AUDIO_OGG,
    JSON,
    TEXT,
    BINARY,
    UNKNOWN
};

/**
 * Content metadata
 */
struct ContentMetadata {
    Hash256 content_hash;
    ContentType type;
    size_t size_bytes;
    std::string mime_type;
    std::chrono::system_clock::time_point last_modified;
    bool is_cacheable{true};
    std::optional<std::string> filename;
};

/**
 * Cached content entry
 */
struct CacheEntry {
    ContentMetadata metadata;
    std::vector<uint8_t> data;
    std::chrono::system_clock::time_point cached_at;
    std::chrono::system_clock::time_point last_accessed;
    size_t access_count{0};
};

/**
 * Content streaming chunk
 */
struct ContentChunk {
    size_t offset;
    size_t length;
    std::vector<uint8_t> data;
    bool is_final{false};
};

/**
 * Content renderer configuration
 */
struct ContentRendererConfig {
    // Cache settings
    size_t max_cache_size_bytes{100 * 1024 * 1024};  // 100 MB
    size_t max_cached_items{1000};
    std::chrono::seconds cache_ttl{3600};  // 1 hour
    
    // Streaming settings
    size_t chunk_size{64 * 1024};  // 64 KB chunks
    bool enable_range_requests{true};
    
    // Security settings
    bool sanitize_html{true};
    bool block_external_scripts{true};
    std::vector<std::string> allowed_origins;
    
    // Performance
    size_t max_concurrent_fetches{10};
    std::chrono::seconds fetch_timeout{30};
};

/**
 * Content fetch callback
 * Used to retrieve content from the P2P network
 */
using ContentFetchCallback = std::function<std::optional<std::vector<uint8_t>>(
    const Hash256& content_hash
)>;

/**
 * Content renderer
 * 
 * Fetches Things from the P2P network, caches them, and serves them
 * to browsers with appropriate HTTP headers and streaming support.
 */
class ContentRenderer {
public:
    /**
     * Construct content renderer
     * @param config Renderer configuration
     */
    explicit ContentRenderer(const ContentRendererConfig& config);
    
    ~ContentRenderer();
    
    /**
     * Set content fetch callback
     * @param callback Function to fetch content from P2P network
     */
    void set_fetch_callback(ContentFetchCallback callback);
    
    /**
     * Render content by hash
     * @param content_hash Hash of content to render
     * @param range Optional byte range for partial content
     * @return Content data and metadata
     */
    struct RenderResult {
        ContentMetadata metadata;
        std::vector<uint8_t> data;
        bool is_partial{false};
        size_t range_start{0};
        size_t range_end{0};
    };
    
    std::optional<RenderResult> render_content(
        const Hash256& content_hash,
        std::optional<std::pair<size_t, size_t>> range = std::nullopt
    );
    
    /**
     * Stream content in chunks
     * @param content_hash Hash of content to stream
     * @param chunk_callback Callback for each chunk
     * @return true if streaming succeeded
     */
    bool stream_content(
        const Hash256& content_hash,
        std::function<void(const ContentChunk&)> chunk_callback
    );
    
    /**
     * Prefetch content into cache
     * @param content_hash Hash to prefetch
     * @return true if prefetch initiated
     */
    bool prefetch(const Hash256& content_hash);
    
    /**
     * Check if content is cached
     * @param content_hash Hash to check
     * @return true if in cache
     */
    bool is_cached(const Hash256& content_hash) const;
    
    /**
     * Invalidate cached content
     * @param content_hash Hash to invalidate (nullopt = clear all)
     */
    void invalidate_cache(std::optional<Hash256> content_hash = std::nullopt);
    
    /**
     * Get cache statistics
     */
    struct CacheStatistics {
        size_t total_items{0};
        size_t total_bytes{0};
        size_t hit_count{0};
        size_t miss_count{0};
        double hit_ratio{0.0};
        size_t eviction_count{0};
    };
    
    CacheStatistics get_cache_stats() const;
    
    /**
     * Detect content type from data
     * @param data Content data
     * @param filename Optional filename hint
     * @return Detected content type
     */
    static ContentType detect_content_type(
        const std::vector<uint8_t>& data,
        const std::optional<std::string>& filename = std::nullopt
    );
    
    /**
     * Get MIME type for content type
     * @param type Content type
     * @return MIME type string
     */
    static std::string get_mime_type(ContentType type);
    
    /**
     * Sanitize HTML content for safe rendering
     * @param html HTML data
     * @return Sanitized HTML
     */
    static std::vector<uint8_t> sanitize_html(const std::vector<uint8_t>& html);

private:
    /**
     * Fetch content from P2P network
     */
    std::optional<std::vector<uint8_t>> fetch_from_network(const Hash256& content_hash);
    
    /**
     * Add to cache
     */
    void add_to_cache(const Hash256& content_hash, const std::vector<uint8_t>& data);
    
    /**
     * Get from cache
     */
    std::optional<CacheEntry> get_from_cache(const Hash256& content_hash);
    
    /**
     * Evict least recently used items
     */
    void evict_lru();
    
    /**
     * Clean expired cache entries
     */
    void cleanup_expired();
    
    /**
     * Extract content metadata
     */
    ContentMetadata extract_metadata(
        const Hash256& content_hash,
        const std::vector<uint8_t>& data
    );
    
    ContentRendererConfig config_;
    ContentFetchCallback fetch_callback_;
    
    // Cache management
    mutable std::mutex cache_mutex_;
    std::unordered_map<Hash256, CacheEntry> cache_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    CacheStatistics stats_;
};

/**
 * HTTP response builder for rendered content
 */
struct ContentHttpResponse {
    int status_code;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    
    /**
     * Build HTTP response from render result
     */
    static ContentHttpResponse from_render_result(
        const ContentRenderer::RenderResult& result,
        bool include_content_length = true
    );
    
    /**
     * Build 404 Not Found response
     */
    static ContentHttpResponse not_found();
    
    /**
     * Build 206 Partial Content response
     */
    static ContentHttpResponse partial_content(
        const ContentRenderer::RenderResult& result,
        size_t total_size
    );
    
    /**
     * Build 416 Range Not Satisfiable response
     */
    static ContentHttpResponse range_not_satisfiable(size_t total_size);
};

} // namespace gateway
} // namespace cashew
