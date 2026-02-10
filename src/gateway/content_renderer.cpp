#include "cashew/gateway/content_renderer.hpp"
#include "../security/content_integrity.hpp"
#include "../utils/logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace cashew {
namespace gateway {

namespace {

std::string hash_to_string(const Hash256& hash) {
    std::stringstream ss;
    for (const auto& byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

// Magic bytes for content type detection
const std::vector<uint8_t> PNG_MAGIC = {0x89, 0x50, 0x4E, 0x47};
const std::vector<uint8_t> JPEG_MAGIC = {0xFF, 0xD8, 0xFF};
const std::vector<uint8_t> GIF_MAGIC = {0x47, 0x49, 0x46};
const std::vector<uint8_t> WEBP_MAGIC = {0x52, 0x49, 0x46, 0x46};

bool starts_with(const std::vector<uint8_t>& data, const std::vector<uint8_t>& prefix) {
    if (data.size() < prefix.size()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), data.begin());
}

bool has_extension(const std::string& filename, const std::string& ext) {
    if (filename.size() < ext.size()) {
        return false;
    }
    return filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0;
}

} // anonymous namespace

ContentRenderer::ContentRenderer(const ContentRendererConfig& config)
    : config_(config)
{}

ContentRenderer::~ContentRenderer() {
    invalidate_cache();
}

void ContentRenderer::set_fetch_callback(ContentFetchCallback callback) {
    fetch_callback_ = std::move(callback);
}

std::optional<ContentRenderer::RenderResult> ContentRenderer::render_content(
    const Hash256& content_hash,
    std::optional<std::pair<size_t, size_t>> range
) {
    // Try cache first
    auto cached = get_from_cache(content_hash);
    
    std::vector<uint8_t> data;
    
    if (cached) {
        CASHEW_LOG_DEBUG("Cache hit for content: {}", hash_to_string(content_hash));
        data = cached->data;
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.hit_count++;
    } else {
        CASHEW_LOG_DEBUG("Cache miss for content: {}", hash_to_string(content_hash));
        
        // Fetch from network
        auto fetched = fetch_from_network(content_hash);
        if (!fetched) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.miss_count++;
            return std::nullopt;
        }
        
        data = *fetched;
        
        // Verify content integrity (defense in depth)
        auto integrity_result = security::ContentIntegrityChecker::verify_content(data, content_hash);
        if (!integrity_result.is_valid) {
            CASHEW_LOG_ERROR("Content integrity verification failed: {}", 
                           integrity_result.error_message);
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.miss_count++;
            return std::nullopt;
        }
        
        CASHEW_LOG_DEBUG("Content integrity verified ({} bytes)", integrity_result.content_size);
        
        // Add to cache
        add_to_cache(content_hash, data);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.miss_count++;
    }
    
    // Extract metadata
    auto metadata = extract_metadata(content_hash, data);
    
    RenderResult result;
    result.metadata = metadata;
    
    // Handle range request
    if (range && config_.enable_range_requests) {
        auto [range_start, range_end] = *range;
        
        // Validate range
        if (range_start >= data.size() || range_end >= data.size() || range_start > range_end) {
            CASHEW_LOG_WARN("Invalid range request: {}-{} for size {}", 
                           range_start, range_end, data.size());
            return std::nullopt;
        }
        
        result.data.assign(data.begin() + range_start, data.begin() + range_end + 1);
        result.is_partial = true;
        result.range_start = range_start;
        result.range_end = range_end;
        
        CASHEW_LOG_DEBUG("Serving partial content: {}-{}/{}", 
                        range_start, range_end, data.size());
    } else {
        result.data = std::move(data);
    }
    
    // Sanitize HTML if enabled
    if (config_.sanitize_html && metadata.type == ContentType::HTML) {
        result.data = sanitize_html(result.data);
    }
    
    return result;
}

bool ContentRenderer::stream_content(
    const Hash256& content_hash,
    std::function<void(const ContentChunk&)> chunk_callback
) {
    if (!chunk_callback) {
        return false;
    }
    
    // Get full content
    auto result = render_content(content_hash);
    if (!result) {
        return false;
    }
    
    const auto& data = result->data;
    size_t offset = 0;
    
    // Stream in chunks
    while (offset < data.size()) {
        size_t chunk_length = std::min(config_.chunk_size, data.size() - offset);
        
        ContentChunk chunk;
        chunk.offset = offset;
        chunk.length = chunk_length;
        chunk.data.assign(data.begin() + offset, data.begin() + offset + chunk_length);
        chunk.is_final = (offset + chunk_length >= data.size());
        
        chunk_callback(chunk);
        
        offset += chunk_length;
    }
    
    CASHEW_LOG_DEBUG("Streamed content {} in {} chunks", 
                    hash_to_string(content_hash), 
                    (data.size() + config_.chunk_size - 1) / config_.chunk_size);
    
    return true;
}

bool ContentRenderer::prefetch(const Hash256& content_hash) {
    // Check if already cached
    if (is_cached(content_hash)) {
        return true;
    }
    
    // Fetch from network
    auto data = fetch_from_network(content_hash);
    if (!data) {
        return false;
    }
    
    // Add to cache
    add_to_cache(content_hash, *data);
    
    CASHEW_LOG_DEBUG("Prefetched content: {}", hash_to_string(content_hash));
    return true;
}

bool ContentRenderer::is_cached(const Hash256& content_hash) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cache_.find(content_hash) != cache_.end();
}

void ContentRenderer::invalidate_cache(std::optional<Hash256> content_hash) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    if (content_hash) {
        auto it = cache_.find(*content_hash);
        if (it != cache_.end()) {
            cache_.erase(it);
            CASHEW_LOG_DEBUG("Invalidated cache for: {}", hash_to_string(*content_hash));
        }
    } else {
        cache_.clear();
        CASHEW_LOG_INFO("Cleared entire content cache");
    }
}

ContentRenderer::CacheStatistics ContentRenderer::get_cache_stats() const {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);
    
    auto stats = stats_;
    stats.total_items = cache_.size();
    
    size_t total_bytes = 0;
    for (const auto& [hash, entry] : cache_) {
        total_bytes += entry.data.size();
    }
    stats.total_bytes = total_bytes;
    
    size_t total_requests = stats.hit_count + stats.miss_count;
    if (total_requests > 0) {
        stats.hit_ratio = static_cast<double>(stats.hit_count) / total_requests;
    }
    
    return stats;
}

ContentType ContentRenderer::detect_content_type(
    const std::vector<uint8_t>& data,
    const std::optional<std::string>& filename
) {
    // Check magic bytes first
    if (starts_with(data, PNG_MAGIC)) {
        return ContentType::IMAGE_PNG;
    }
    if (starts_with(data, JPEG_MAGIC)) {
        return ContentType::IMAGE_JPEG;
    }
    if (starts_with(data, GIF_MAGIC)) {
        return ContentType::IMAGE_GIF;
    }
    if (starts_with(data, WEBP_MAGIC)) {
        return ContentType::IMAGE_WEBP;
    }
    
    // Check filename extension
    if (filename) {
        const auto& fname = *filename;
        
        if (has_extension(fname, ".html") || has_extension(fname, ".htm")) {
            return ContentType::HTML;
        }
        if (has_extension(fname, ".js")) {
            return ContentType::JAVASCRIPT;
        }
        if (has_extension(fname, ".css")) {
            return ContentType::CSS;
        }
        if (has_extension(fname, ".json")) {
            return ContentType::JSON;
        }
        if (has_extension(fname, ".txt")) {
            return ContentType::TEXT;
        }
        if (has_extension(fname, ".mp4")) {
            return ContentType::VIDEO_MP4;
        }
        if (has_extension(fname, ".webm")) {
            return ContentType::VIDEO_WEBM;
        }
        if (has_extension(fname, ".mp3")) {
            return ContentType::AUDIO_MP3;
        }
        if (has_extension(fname, ".ogg")) {
            return ContentType::AUDIO_OGG;
        }
    }
    
    // Check if looks like text
    if (data.size() > 0 && data.size() < 1024) {
        bool is_text = true;
        for (uint8_t byte : data) {
            if (byte < 32 && byte != '\n' && byte != '\r' && byte != '\t') {
                is_text = false;
                break;
            }
        }
        if (is_text) {
            return ContentType::TEXT;
        }
    }
    
    return ContentType::BINARY;
}

std::string ContentRenderer::get_mime_type(ContentType type) {
    switch (type) {
        case ContentType::HTML: return "text/html; charset=utf-8";
        case ContentType::JAVASCRIPT: return "application/javascript; charset=utf-8";
        case ContentType::CSS: return "text/css; charset=utf-8";
        case ContentType::IMAGE_PNG: return "image/png";
        case ContentType::IMAGE_JPEG: return "image/jpeg";
        case ContentType::IMAGE_GIF: return "image/gif";
        case ContentType::IMAGE_WEBP: return "image/webp";
        case ContentType::VIDEO_MP4: return "video/mp4";
        case ContentType::VIDEO_WEBM: return "video/webm";
        case ContentType::AUDIO_MP3: return "audio/mpeg";
        case ContentType::AUDIO_OGG: return "audio/ogg";
        case ContentType::JSON: return "application/json; charset=utf-8";
        case ContentType::TEXT: return "text/plain; charset=utf-8";
        case ContentType::BINARY: return "application/octet-stream";
        default: return "application/octet-stream";
    }
}

std::vector<uint8_t> ContentRenderer::sanitize_html(const std::vector<uint8_t>& html) {
    // Comprehensive HTML sanitization to prevent XSS attacks
    // 
    // PRODUCTION NOTE: This is a basic sanitizer for demonstration.
    // For production use, integrate a proper HTML sanitization library like:
    // - google/gumbo-parser with allowlist-based sanitization
    // - htmlcxx or similar HTML parser with security focus
    // - Server-side Content Security Policy (CSP) headers (already implemented)
    
    std::string html_str(html.begin(), html.end());
    
    // 1. Remove <script> tags (case-insensitive)
    size_t pos = 0;
    while ((pos = html_str.find("<script", pos)) != std::string::npos) {
        // Make case-insensitive by checking lowercase
        if (pos == 0 || !std::isalnum(html_str[pos - 1])) {
            size_t end = html_str.find("</script>", pos);
            if (end != std::string::npos) {
                html_str.erase(pos, end - pos + 9);
            } else {
                // No closing tag, remove to end
                html_str.erase(pos);
                break;
            }
        } else {
            pos++;
        }
    }
    
    // 2. Remove <iframe> tags (can embed arbitrary content)
    pos = 0;
    while ((pos = html_str.find("<iframe", pos)) != std::string::npos) {
        size_t end = html_str.find("</iframe>", pos);
        if (end != std::string::npos) {
            html_str.erase(pos, end - pos + 9);
        } else {
            // Try self-closing or no closing tag
            size_t bracket = html_str.find('>', pos);
            if (bracket != std::string::npos) {
                html_str.erase(pos, bracket - pos + 1);
            }
        }
    }
    
    // 3. Remove <object>, <embed>, <applet> tags
    const std::vector<std::string> dangerous_tags = {
        "<object", "<embed", "<applet"
    };
    
    for (const auto& tag : dangerous_tags) {
        pos = 0;
        while ((pos = html_str.find(tag, pos)) != std::string::npos) {
            size_t end = html_str.find('>', pos);
            if (end != std::string::npos) {
                html_str.erase(pos, end - pos + 1);
            } else {
                pos++;
            }
        }
    }
    
    // 4. Remove all event handlers (onclick, onerror, onload, etc.)
    const std::vector<std::string> event_attrs = {
        "onclick", "ondblclick", "onmousedown", "onmouseup", "onmouseover", "onmouseout",
        "onmousemove", "onmouseenter", "onmouseleave", "onkeydown", "onkeyup", "onkeypress",
        "onload", "onunload", "onerror", "onabort", "onblur", "onchange", "onfocus",
        "onreset", "onselect", "onsubmit", "onscroll", "onresize", "ondrag", "ondrop",
        "oncontextmenu", "oninput", "oninvalid", "onsearch"
    };
    
    for (const auto& attr : event_attrs) {
        pos = 0;
        while ((pos = html_str.find(attr, pos)) != std::string::npos) {
            // Check if it's actually an attribute (preceded by space or tag start)
            if (pos > 0 && !std::isspace(html_str[pos - 1]) && html_str[pos - 1] != '<') {
                pos++;
                continue;
            }
            
            // Find the attribute value
            size_t eq = html_str.find('=', pos);
            if (eq != std::string::npos && eq - pos < 20) {  // Reasonable distance
                size_t quote_start = html_str.find_first_of("\"'", eq);
                if (quote_start != std::string::npos) {
                    char quote = html_str[quote_start];
                    size_t quote_end = html_str.find(quote, quote_start + 1);
                    if (quote_end != std::string::npos) {
                        html_str.erase(pos, quote_end - pos + 1);
                        continue;
                    }
                }
            }
            pos++;
        }
    }
    
    // 5. Remove javascript: URLs
    pos = 0;
    while ((pos = html_str.find("javascript:", pos)) != std::string::npos) {
        // Find containing attribute
        size_t attr_start = html_str.rfind('"', pos);
        size_t attr_end = html_str.find('"', pos);
        
        if (attr_start != std::string::npos && attr_end != std::string::npos) {
            html_str.erase(attr_start, attr_end - attr_start + 1);
            html_str.insert(attr_start, "\"\"");  // Replace with empty string
        } else {
            pos++;
        }
    }
    
    // 6. Remove data: URLs (can contain base64-encoded scripts)
    pos = 0;
    while ((pos = html_str.find("data:", pos)) != std::string::npos) {
        // Allow data: URLs only for images
        size_t check_start = (pos > 20) ? pos - 20 : 0;
        std::string context = html_str.substr(check_start, pos - check_start + 10);
        
        // If not in src attribute of img tag, remove it
        if (context.find("<img") == std::string::npos && 
            context.find("src=") == std::string::npos) {
            size_t attr_start = html_str.rfind('"', pos);
            size_t attr_end = html_str.find('"', pos);
            
            if (attr_start != std::string::npos && attr_end != std::string::npos) {
                html_str.erase(attr_start, attr_end - attr_start + 1);
                html_str.insert(attr_start, "\"\"");
            }
        }
        pos++;
    }
    
    // 7. Remove dangerous style attributes (expression(), behavior:url(), etc.)
    pos = 0;
    while ((pos = html_str.find("style=", pos)) != std::string::npos) {
        size_t quote_start = html_str.find_first_of("\"'", pos + 6);
        if (quote_start != std::string::npos) {
            char quote = html_str[quote_start];
            size_t quote_end = html_str.find(quote, quote_start + 1);
            
            if (quote_end != std::string::npos) {
                std::string style_content = html_str.substr(quote_start + 1, 
                                                           quote_end - quote_start - 1);
                
                // Check for dangerous CSS
                if (style_content.find("expression") != std::string::npos ||
                    style_content.find("behavior") != std::string::npos ||
                    style_content.find("javascript:") != std::string::npos ||
                    style_content.find("@import") != std::string::npos) {
                    // Remove entire style attribute
                    html_str.erase(pos, quote_end - pos + 1);
                    continue;
                }
            }
        }
        pos++;
    }
    
    // 8. Add sandbox attribute to any remaining iframes (defense in depth)
    pos = 0;
    while ((pos = html_str.find("<iframe", pos)) != std::string::npos) {
        size_t end = html_str.find('>', pos);
        if (end != std::string::npos) {
            // Check if sandbox already present
            std::string tag = html_str.substr(pos, end - pos);
            if (tag.find("sandbox") == std::string::npos) {
                html_str.insert(end, " sandbox=\"\"");
            }
        }
        pos++;
    }
    
    CASHEW_LOG_DEBUG("HTML sanitized: {} bytes -> {} bytes", 
                    html.size(), html_str.size());
    
    return std::vector<uint8_t>(html_str.begin(), html_str.end());
}

std::optional<std::vector<uint8_t>> ContentRenderer::fetch_from_network(
    const Hash256& content_hash
) {
    if (!fetch_callback_) {
        CASHEW_LOG_ERROR("No fetch callback configured");
        return std::nullopt;
    }
    
    CASHEW_LOG_DEBUG("Fetching content from network: {}", hash_to_string(content_hash));
    
    try {
        return fetch_callback_(content_hash);
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Failed to fetch content: {}", e.what());
        return std::nullopt;
    }
}

void ContentRenderer::add_to_cache(const Hash256& content_hash, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check if we need to evict
    size_t current_size = 0;
    for (const auto& [hash, entry] : cache_) {
        current_size += entry.data.size();
    }
    
    if (current_size + data.size() > config_.max_cache_size_bytes ||
        cache_.size() >= config_.max_cached_items) {
        evict_lru();
    }
    
    CacheEntry entry;
    entry.metadata = extract_metadata(content_hash, data);
    entry.data = data;
    entry.cached_at = std::chrono::system_clock::now();
    entry.last_accessed = entry.cached_at;
    entry.access_count = 0;
    
    cache_[content_hash] = std::move(entry);
    
    CASHEW_LOG_DEBUG("Added to cache: {} ({} bytes)", 
                    hash_to_string(content_hash), data.size());
}

std::optional<CacheEntry> ContentRenderer::get_from_cache(const Hash256& content_hash) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = cache_.find(content_hash);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    
    // Update access info
    it->second.last_accessed = std::chrono::system_clock::now();
    it->second.access_count++;
    
    return it->second;
}

void ContentRenderer::evict_lru() {
    if (cache_.empty()) {
        return;
    }
    
    // Find least recently used
    auto lru_it = cache_.begin();
    auto oldest = lru_it->second.last_accessed;
    
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.last_accessed < oldest) {
            oldest = it->second.last_accessed;
            lru_it = it;
        }
    }
    
    CASHEW_LOG_DEBUG("Evicting LRU cache entry: {}", hash_to_string(lru_it->first));
    cache_.erase(lru_it);
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.eviction_count++;
}

void ContentRenderer::cleanup_expired() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto now = std::chrono::system_clock::now();
    
    for (auto it = cache_.begin(); it != cache_.end();) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.cached_at
        );
        
        if (age > config_.cache_ttl) {
            CASHEW_LOG_DEBUG("Removing expired cache entry: {}", hash_to_string(it->first));
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

ContentMetadata ContentRenderer::extract_metadata(
    const Hash256& content_hash,
    const std::vector<uint8_t>& data
) {
    ContentMetadata metadata;
    metadata.content_hash = content_hash;
    metadata.size_bytes = data.size();
    metadata.type = detect_content_type(data);
    metadata.mime_type = get_mime_type(metadata.type);
    metadata.last_modified = std::chrono::system_clock::now();
    metadata.is_cacheable = true;
    
    return metadata;
}

// ===== ContentHttpResponse Implementation =====

ContentHttpResponse ContentHttpResponse::from_render_result(
    const ContentRenderer::RenderResult& result,
    bool include_content_length
) {
    ContentHttpResponse response;
    response.status_code = 200;
    response.body = result.data;
    
    response.headers["Content-Type"] = result.metadata.mime_type;
    
    if (include_content_length) {
        response.headers["Content-Length"] = std::to_string(result.data.size());
    }
    
    // Add cache headers
    if (result.metadata.is_cacheable) {
        response.headers["Cache-Control"] = "public, max-age=3600";
        response.headers["ETag"] = "\"" + hash_to_string(result.metadata.content_hash) + "\"";
    } else {
        response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate";
    }
    
    return response;
}

ContentHttpResponse ContentHttpResponse::not_found() {
    ContentHttpResponse response;
    response.status_code = 404;
    
    std::string body = R"({"error": "Content not found"})";
    response.body.assign(body.begin(), body.end());
    response.headers["Content-Type"] = "application/json";
    response.headers["Content-Length"] = std::to_string(body.size());
    
    return response;
}

ContentHttpResponse ContentHttpResponse::partial_content(
    const ContentRenderer::RenderResult& result,
    size_t total_size
) {
    ContentHttpResponse response;
    response.status_code = 206;
    response.body = result.data;
    
    response.headers["Content-Type"] = result.metadata.mime_type;
    response.headers["Content-Length"] = std::to_string(result.data.size());
    
    std::stringstream range_header;
    range_header << "bytes " << result.range_start << "-" 
                 << result.range_end << "/" << total_size;
    response.headers["Content-Range"] = range_header.str();
    response.headers["Accept-Ranges"] = "bytes";
    
    return response;
}

ContentHttpResponse ContentHttpResponse::range_not_satisfiable(size_t total_size) {
    ContentHttpResponse response;
    response.status_code = 416;
    
    response.headers["Content-Range"] = "bytes */" + std::to_string(total_size);
    
    return response;
}

} // namespace gateway
} // namespace cashew
