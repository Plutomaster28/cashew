#include "network/router.hpp"
#include "crypto/blake3.hpp"
#include "crypto/random.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace cashew::network {

// RoutingEntry methods

bool RoutingEntry::is_stale() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_timestamp = static_cast<uint64_t>(now_time_t);
    
    static constexpr uint64_t ENTRY_TTL_SECONDS = 3600;  // 1 hour
    uint64_t age = current_timestamp - last_seen_timestamp;
    return age > ENTRY_TTL_SECONDS;
}

// ContentRequest methods

std::vector<uint8_t> ContentRequest::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Content hash (32 bytes)
    data.insert(data.end(), content_hash.hash.begin(), content_hash.hash.end());
    
    // Requester ID (32 bytes)
    data.insert(data.end(), requester_id.id.begin(), requester_id.id.end());
    
    // Request ID (32 bytes)
    data.insert(data.end(), request_id.begin(), request_id.end());
    
    // Hop limit (1 byte)
    data.push_back(hop_limit);
    
    // Timestamp (8 bytes)
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>(timestamp >> (i * 8)));
    }
    
    // Onion layers count (2 bytes)
    uint16_t layer_count = static_cast<uint16_t>(onion_layers.size());
    data.push_back(static_cast<uint8_t>(layer_count & 0xFF));
    data.push_back(static_cast<uint8_t>((layer_count >> 8) & 0xFF));
    
    // Each onion layer (length + data)
    for (const auto& layer : onion_layers) {
        uint32_t layer_size = static_cast<uint32_t>(layer.size());
        for (int i = 0; i < 4; ++i) {
            data.push_back(static_cast<uint8_t>(layer_size >> (i * 8)));
        }
        data.insert(data.end(), layer.begin(), layer.end());
    }
    
    return data;
}

std::optional<ContentRequest> ContentRequest::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 32 + 32 + 32 + 1 + 8 + 2) {
        CASHEW_LOG_ERROR("ContentRequest too small");
        return std::nullopt;
    }
    
    ContentRequest req;
    size_t offset = 0;
    
    // Content hash
    std::copy(data.begin() + offset, data.begin() + offset + 32, req.content_hash.hash.begin());
    offset += 32;
    
    // Requester ID
    std::copy(data.begin() + offset, data.begin() + offset + 32, req.requester_id.id.begin());
    offset += 32;
    
    // Request ID
    std::copy(data.begin() + offset, data.begin() + offset + 32, req.request_id.begin());
    offset += 32;
    
    // Hop limit
    req.hop_limit = data[offset++];
    
    // Timestamp
    req.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        req.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Onion layers count
    uint16_t layer_count = data[offset] | (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;
    
    // Read onion layers
    for (uint16_t i = 0; i < layer_count; ++i) {
        if (offset + 4 > data.size()) {
            CASHEW_LOG_ERROR("Invalid onion layer format");
            return std::nullopt;
        }
        
        uint32_t layer_size = 0;
        for (int j = 0; j < 4; ++j) {
            layer_size |= static_cast<uint32_t>(data[offset++]) << (j * 8);
        }
        
        if (offset + layer_size > data.size()) {
            CASHEW_LOG_ERROR("Invalid onion layer size");
            return std::nullopt;
        }
        
        std::vector<uint8_t> layer(data.begin() + offset, data.begin() + offset + layer_size);
        req.onion_layers.push_back(layer);
        offset += layer_size;
    }
    
    return req;
}

Hash256 ContentRequest::compute_id() const {
    std::vector<uint8_t> id_data;
    id_data.insert(id_data.end(), content_hash.hash.begin(), content_hash.hash.end());
    id_data.insert(id_data.end(), requester_id.id.begin(), requester_id.id.end());
    
    // Add timestamp for uniqueness
    for (int i = 0; i < 8; ++i) {
        id_data.push_back(static_cast<uint8_t>(timestamp >> (i * 8)));
    }
    
    return crypto::Blake3::hash(id_data);
}

// ContentResponse methods

std::vector<uint8_t> ContentResponse::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Content hash (32 bytes)
    data.insert(data.end(), content_hash.hash.begin(), content_hash.hash.end());
    
    // Hosting node (32 bytes)
    data.insert(data.end(), hosting_node.id.begin(), hosting_node.id.end());
    
    // Request ID (32 bytes)
    data.insert(data.end(), request_id.begin(), request_id.end());
    
    // Hop count (1 byte)
    data.push_back(hop_count);
    
    // Content data size (4 bytes)
    uint32_t content_size = static_cast<uint32_t>(content_data.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>(content_size >> (i * 8)));
    }
    
    // Content data
    data.insert(data.end(), content_data.begin(), content_data.end());
    
    // Signature (64 bytes)
    data.insert(data.end(), signature.begin(), signature.end());
    
    return data;
}

std::optional<ContentResponse> ContentResponse::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 32 + 32 + 32 + 1 + 4 + 64) {
        CASHEW_LOG_ERROR("ContentResponse too small");
        return std::nullopt;
    }
    
    ContentResponse resp;
    size_t offset = 0;
    
    // Content hash
    std::copy(data.begin() + offset, data.begin() + offset + 32, resp.content_hash.hash.begin());
    offset += 32;
    
    // Hosting node
    std::copy(data.begin() + offset, data.begin() + offset + 32, resp.hosting_node.id.begin());
    offset += 32;
    
    // Request ID
    std::copy(data.begin() + offset, data.begin() + offset + 32, resp.request_id.begin());
    offset += 32;
    
    // Hop count
    resp.hop_count = data[offset++];
    
    // Content data size
    uint32_t content_size = 0;
    for (int i = 0; i < 4; ++i) {
        content_size |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    // Validate content size
    if (offset + content_size + 64 > data.size()) {
        CASHEW_LOG_ERROR("Invalid content size in response");
        return std::nullopt;
    }
    
    // Content data
    resp.content_data.assign(data.begin() + offset, data.begin() + offset + content_size);
    offset += content_size;
    
    // Signature
    std::copy(data.begin() + offset, data.begin() + offset + 64, resp.signature.begin());
    
    return resp;
}

bool ContentResponse::verify_signature(const PublicKey& host_public_key) const {
    // TODO: Implement Ed25519 signature verification
    // Need to create signed data (content_hash + content_data)
    // and verify with host_public_key
    (void)host_public_key;
    CASHEW_LOG_WARN("ContentResponse signature verification not yet implemented");
    return true;
}

// RoutingTable methods

void RoutingTable::add_node(const NodeID& node_id, uint8_t hop_distance) {
    auto it = entries_.find(node_id);
    
    if (it == entries_.end()) {
        // New entry
        RoutingEntry entry(node_id, hop_distance);
        
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        entry.last_seen_timestamp = static_cast<uint64_t>(now_time_t);
        
        entries_[node_id] = entry;
        CASHEW_LOG_DEBUG("Added routing entry for node at {} hops", hop_distance);
    } else {
        // Update existing entry
        if (hop_distance < it->second.hop_distance) {
            it->second.hop_distance = hop_distance;
            CASHEW_LOG_DEBUG("Updated routing entry hop distance to {}", hop_distance);
        }
        update_node_seen(node_id);
    }
}

void RoutingTable::remove_node(const NodeID& node_id) {
    auto it = entries_.find(node_id);
    if (it == entries_.end()) {
        return;
    }
    
    // Remove from content index
    for (const auto& content_hash : it->second.advertised_content) {
        auto content_it = content_index_.find(content_hash);
        if (content_it != content_index_.end()) {
            auto& hosts = content_it->second;
            hosts.erase(std::remove(hosts.begin(), hosts.end(), node_id), hosts.end());
            
            if (hosts.empty()) {
                content_index_.erase(content_it);
            }
        }
    }
    
    entries_.erase(it);
    CASHEW_LOG_DEBUG("Removed routing entry for node");
}

void RoutingTable::update_node_seen(const NodeID& node_id) {
    auto it = entries_.find(node_id);
    if (it == entries_.end()) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    it->second.last_seen_timestamp = static_cast<uint64_t>(now_time_t);
}

void RoutingTable::update_node_reliability(const NodeID& node_id, float score) {
    auto it = entries_.find(node_id);
    if (it == entries_.end()) {
        return;
    }
    
    // Exponential moving average
    it->second.reliability_score = 0.7f * it->second.reliability_score + 0.3f * score;
    
    // Clamp
    if (it->second.reliability_score < 0.0f) {
        it->second.reliability_score = 0.0f;
    }
    if (it->second.reliability_score > 1.0f) {
        it->second.reliability_score = 1.0f;
    }
    
    CASHEW_LOG_DEBUG("Updated node reliability to {:.2f}", it->second.reliability_score);
}

std::optional<RoutingEntry> RoutingTable::get_entry(const NodeID& node_id) const {
    auto it = entries_.find(node_id);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<RoutingEntry> RoutingTable::get_all_entries() const {
    std::vector<RoutingEntry> result;
    result.reserve(entries_.size());
    
    for (const auto& [node_id, entry] : entries_) {
        result.push_back(entry);
    }
    
    return result;
}

void RoutingTable::advertise_content(const NodeID& node_id, const ContentHash& content_hash) {
    // Add to node's advertised content
    auto it = entries_.find(node_id);
    if (it != entries_.end()) {
        auto& advertised = it->second.advertised_content;
        if (std::find(advertised.begin(), advertised.end(), content_hash) == advertised.end()) {
            advertised.push_back(content_hash);
        }
    }
    
    // Add to content index
    auto& hosts = content_index_[content_hash];
    if (std::find(hosts.begin(), hosts.end(), node_id) == hosts.end()) {
        hosts.push_back(node_id);
        CASHEW_LOG_DEBUG("Node now advertising content (total {} hosts)", hosts.size());
    }
}

void RoutingTable::remove_content_advertisement(const NodeID& node_id, const ContentHash& content_hash) {
    // Remove from node's advertised content
    auto it = entries_.find(node_id);
    if (it != entries_.end()) {
        auto& advertised = it->second.advertised_content;
        advertised.erase(std::remove(advertised.begin(), advertised.end(), content_hash), advertised.end());
    }
    
    // Remove from content index
    auto content_it = content_index_.find(content_hash);
    if (content_it != content_index_.end()) {
        auto& hosts = content_it->second;
        hosts.erase(std::remove(hosts.begin(), hosts.end(), node_id), hosts.end());
        
        if (hosts.empty()) {
            content_index_.erase(content_it);
        }
    }
}

std::vector<NodeID> RoutingTable::find_hosts_for_content(const ContentHash& content_hash) const {
    auto it = content_index_.find(content_hash);
    if (it == content_index_.end()) {
        return {};
    }
    return it->second;
}

bool RoutingTable::has_content_route(const ContentHash& content_hash) const {
    auto it = content_index_.find(content_hash);
    return it != content_index_.end() && !it->second.empty();
}

std::optional<NodeID> RoutingTable::select_best_host(const ContentHash& content_hash) const {
    auto hosts = find_hosts_for_content(content_hash);
    if (hosts.empty()) {
        return std::nullopt;
    }
    
    // Find host with best score (lowest hops * highest reliability)
    NodeID best_host = hosts[0];
    float best_score = -1.0f;
    
    for (const auto& host : hosts) {
        auto entry_opt = get_entry(host);
        if (!entry_opt) continue;
        
        const auto& entry = *entry_opt;
        
        // Skip stale or unreliable entries
        if (entry.is_stale() || entry.reliability_score < MIN_RELIABILITY_SCORE) {
            continue;
        }
        
        // Score: higher is better (lower hops, higher reliability)
        float score = entry.reliability_score / (1.0f + static_cast<float>(entry.hop_distance));
        
        if (score > best_score) {
            best_score = score;
            best_host = host;
        }
    }
    
    if (best_score < 0.0f) {
        return std::nullopt;
    }
    
    return best_host;
}

std::vector<NodeID> RoutingTable::select_multiple_hosts(const ContentHash& content_hash, size_t count) const {
    auto hosts = find_hosts_for_content(content_hash);
    if (hosts.empty()) {
        return {};
    }
    
    // Score all hosts
    struct ScoredHost {
        NodeID node_id;
        float score;
    };
    
    std::vector<ScoredHost> scored_hosts;
    scored_hosts.reserve(hosts.size());
    
    for (const auto& host : hosts) {
        auto entry_opt = get_entry(host);
        if (!entry_opt) continue;
        
        const auto& entry = *entry_opt;
        
        // Skip stale or unreliable entries
        if (entry.is_stale() || entry.reliability_score < MIN_RELIABILITY_SCORE) {
            continue;
        }
        
        float score = entry.reliability_score / (1.0f + static_cast<float>(entry.hop_distance));
        scored_hosts.push_back({host, score});
    }
    
    // Sort by score (descending)
    std::sort(scored_hosts.begin(), scored_hosts.end(),
        [](const ScoredHost& a, const ScoredHost& b) {
            return a.score > b.score;
        });
    
    // Return top N
    std::vector<NodeID> result;
    size_t num_results = std::min(count, scored_hosts.size());
    result.reserve(num_results);
    
    for (size_t i = 0; i < num_results; ++i) {
        result.push_back(scored_hosts[i].node_id);
    }
    
    return result;
}

void RoutingTable::cleanup_stale_entries() {
    std::vector<NodeID> to_remove;
    
    for (const auto& [node_id, entry] : entries_) {
        if (entry.is_stale()) {
            to_remove.push_back(node_id);
        }
    }
    
    for (const auto& node_id : to_remove) {
        remove_node(node_id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_DEBUG("Cleaned up {} stale routing entries", to_remove.size());
    }
}

// PendingRequest methods

bool PendingRequest::has_timed_out() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    uint64_t current_timestamp = static_cast<uint64_t>(now_time_t);
    
    uint64_t age = current_timestamp - timestamp;
    return age > TIMEOUT_SECONDS;
}

// Router methods

Router::Router(const NodeID& local_node_id)
    : local_node_id_(local_node_id),
      routing_table_(),
      requests_sent_(0),
      requests_received_(0),
      responses_sent_(0),
      responses_received_(0),
      forwards_(0),
      next_request_counter_(1)
{
    CASHEW_LOG_INFO("Router initialized for local node");
}

Hash256 Router::request_content(const ContentHash& content_hash, uint8_t hop_limit) {
    // Create request
    ContentRequest request;
    request.content_hash = content_hash;
    request.requester_id = local_node_id_;
    request.request_id = generate_request_id();
    request.hop_limit = std::min(hop_limit, ContentRequest::MAX_HOP_LIMIT);
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    request.timestamp = static_cast<uint64_t>(now_time_t);
    
    // Track request
    PendingRequest pending;
    pending.request_id = request.request_id;
    pending.content_hash = content_hash;
    pending.original_requester = local_node_id_;
    pending.timestamp = request.timestamp;
    pending.retries = 0;
    
    pending_requests_[request.request_id] = pending;
    
    // Find next hop
    auto next_hop_opt = select_next_hop(content_hash);
    if (!next_hop_opt.id.empty()) {
        send_request_to_peer(next_hop_opt, request);
        requests_sent_++;
        CASHEW_LOG_DEBUG("Sent content request (hop limit {})", hop_limit);
    } else {
        CASHEW_LOG_WARN("No route found for content request");
        if (content_not_found_callback_) {
            content_not_found_callback_(content_hash);
        }
    }
    
    return request.request_id;
}

Hash256 Router::request_content_with_onion_routing(
    const ContentHash& content_hash,
    const std::vector<NodeID>& route_path
) {
    // Create request
    ContentRequest request;
    request.content_hash = content_hash;
    request.requester_id = local_node_id_;
    request.request_id = generate_request_id();
    request.hop_limit = static_cast<uint8_t>(route_path.size() + 2);
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    request.timestamp = static_cast<uint64_t>(now_time_t);
    
    // Create onion layers
    std::vector<uint8_t> payload = request.to_bytes();
    request.onion_layers = create_onion_layers(route_path, payload);
    
    // Track request
    PendingRequest pending;
    pending.request_id = request.request_id;
    pending.content_hash = content_hash;
    pending.original_requester = local_node_id_;
    pending.timestamp = request.timestamp;
    pending.retries = 0;
    
    pending_requests_[request.request_id] = pending;
    
    // Send to first hop in route
    if (!route_path.empty()) {
        send_request_to_peer(route_path[0], request);
        requests_sent_++;
        CASHEW_LOG_DEBUG("Sent onion-routed content request ({} hops)", route_path.size());
    }
    
    return request.request_id;
}

void Router::handle_content_request(const ContentRequest& request) {
    requests_received_++;
    
    // Check if hop limit exceeded
    if (request.hop_limit == 0) {
        CASHEW_LOG_WARN("Content request hop limit exceeded, dropping");
        return;
    }
    
    // Decrypt onion layer if present
    if (!request.onion_layers.empty()) {
        auto decrypted_opt = decrypt_onion_layer(request.onion_layers[0]);
        if (!decrypted_opt) {
            CASHEW_LOG_ERROR("Failed to decrypt onion layer");
            return;
        }
        
        // Parse decrypted layer as new request
        auto next_request_opt = ContentRequest::from_bytes(*decrypted_opt);
        if (!next_request_opt) {
            CASHEW_LOG_ERROR("Invalid decrypted request");
            return;
        }
        
        // Forward with remaining layers
        handle_content_request(*next_request_opt);
        return;
    }
    
    // Check if we can serve locally
    if (can_serve_locally(request.content_hash)) {
        CASHEW_LOG_DEBUG("Serving content request locally");
        
        // TODO: Load actual content from storage
        std::vector<uint8_t> content_data;  // Placeholder
        
        ContentResponse response;
        response.content_hash = request.content_hash;
        response.content_data = content_data;
        response.hosting_node = local_node_id_;
        response.request_id = request.request_id;
        response.hop_count = 0;
        
        // TODO: Sign response
        response.signature = {};  // Placeholder
        
        send_response_to_peer(request.requester_id, response);
        responses_sent_++;
        return;
    }
    
    // Should we forward?
    if (!should_forward_request(request)) {
        CASHEW_LOG_DEBUG("Not forwarding request (policy check failed)");
        return;
    }
    
    // Forward to next hop
    auto next_hop = select_next_hop(request.content_hash);
    if (next_hop.id.empty()) {
        CASHEW_LOG_WARN("No route to forward content request");
        return;
    }
    
    // Create forwarded request with decremented hop limit
    ContentRequest forwarded = request;
    forwarded.hop_limit--;
    
    send_request_to_peer(next_hop, forwarded);
    forwards_++;
    CASHEW_LOG_DEBUG("Forwarded content request (remaining hops: {})", forwarded.hop_limit);
}

void Router::handle_content_response(const ContentResponse& response) {
    responses_received_++;
    
    // Check if this is for one of our pending requests
    auto it = pending_requests_.find(response.request_id);
    if (it != pending_requests_.end()) {
        CASHEW_LOG_DEBUG("Received response for our request (hops: {})", response.hop_count);
        
        // Verify content hash
        auto received_hash = crypto::Blake3::hash(response.content_data);
        if (received_hash != response.content_hash.hash) {
            CASHEW_LOG_ERROR("Content hash mismatch in response!");
            return;
        }
        
        // TODO: Verify signature
        
        // Deliver to callback
        if (content_received_callback_) {
            content_received_callback_(response.content_hash, response.content_data);
        }
        
        // Update reliability score for hosting node
        routing_table_.update_node_reliability(response.hosting_node, 1.0f);
        
        // Remove from pending
        pending_requests_.erase(it);
    } else {
        CASHEW_LOG_DEBUG("Received response for unknown request, ignoring");
    }
}

void Router::update_routing_table(const NodeID& node_id, uint8_t hop_distance) {
    routing_table_.add_node(node_id, hop_distance);
}

void Router::advertise_local_content(const ContentHash& content_hash) {
    // Add to local content list
    if (std::find(local_content_.begin(), local_content_.end(), content_hash) == local_content_.end()) {
        local_content_.push_back(content_hash);
        CASHEW_LOG_DEBUG("Advertising local content (total: {})", local_content_.size());
    }
    
    // Add to routing table
    routing_table_.advertise_content(local_node_id_, content_hash);
}

void Router::remove_local_content(const ContentHash& content_hash) {
    local_content_.erase(
        std::remove(local_content_.begin(), local_content_.end(), content_hash),
        local_content_.end()
    );
    
    routing_table_.remove_content_advertisement(local_node_id_, content_hash);
}

std::optional<PendingRequest> Router::get_pending_request(const Hash256& request_id) const {
    auto it = pending_requests_.find(request_id);
    if (it == pending_requests_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Router::cancel_request(const Hash256& request_id) {
    pending_requests_.erase(request_id);
}

void Router::cleanup_timed_out_requests() {
    std::vector<Hash256> to_remove;
    
    for (const auto& [request_id, pending] : pending_requests_) {
        if (pending.has_timed_out()) {
            to_remove.push_back(request_id);
            
            // Notify callback
            if (content_not_found_callback_) {
                content_not_found_callback_(pending.content_hash);
            }
        }
    }
    
    for (const auto& request_id : to_remove) {
        pending_requests_.erase(request_id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_DEBUG("Cleaned up {} timed-out requests", to_remove.size());
    }
}

void Router::update_statistics() {
    routing_table_.cleanup_stale_entries();
    cleanup_timed_out_requests();
}

Hash256 Router::generate_request_id() {
    std::vector<uint8_t> id_data;
    
    // Local node ID
    id_data.insert(id_data.end(), local_node_id_.id.begin(), local_node_id_.id.end());
    
    // Counter
    uint64_t counter = next_request_counter_++;
    for (int i = 0; i < 8; ++i) {
        id_data.push_back(static_cast<uint8_t>(counter >> (i * 8)));
    }
    
    // Random bytes
    auto random_bytes = crypto::Random::generate(16);
    id_data.insert(id_data.end(), random_bytes.begin(), random_bytes.end());
    
    return crypto::Blake3::hash(id_data);
}

NodeID Router::select_next_hop(const ContentHash& content_hash) const {
    auto best_host_opt = routing_table_.select_best_host(content_hash);
    if (best_host_opt) {
        return *best_host_opt;
    }
    
    // No specific route, return empty
    return NodeID{};
}

bool Router::should_forward_request(const ContentRequest& request) const {
    // Don't forward if hop limit is too low
    if (request.hop_limit <= 1) {
        return false;
    }
    
    // Don't forward requests from ourselves
    if (request.requester_id == local_node_id_) {
        return false;
    }
    
    // Check if we have a route
    return routing_table_.has_content_route(request.content_hash);
}

bool Router::can_serve_locally(const ContentHash& content_hash) const {
    return std::find(local_content_.begin(), local_content_.end(), content_hash) != local_content_.end();
}

std::vector<std::vector<uint8_t>> Router::create_onion_layers(
    const std::vector<NodeID>& route_path,
    const std::vector<uint8_t>& payload
) {
    // TODO: Implement actual onion routing encryption
    // This would require X25519 key exchange with each hop
    // For now, return empty (onion routing disabled)
    (void)route_path;
    (void)payload;
    CASHEW_LOG_WARN("Onion routing encryption not yet implemented");
    return {};
}

std::optional<std::vector<uint8_t>> Router::decrypt_onion_layer(
    const std::vector<uint8_t>& encrypted_layer
) {
    // TODO: Implement onion layer decryption
    // Would use our node's private key to decrypt
    (void)encrypted_layer;
    CASHEW_LOG_WARN("Onion layer decryption not yet implemented");
    return std::nullopt;
}

void Router::send_request_to_peer(const NodeID& peer_id, const ContentRequest& request) {
    // TODO: Integrate with SessionManager to actually send over network
    (void)peer_id;
    (void)request;
    CASHEW_LOG_DEBUG("Would send request to peer (network integration pending)");
}

void Router::send_response_to_peer(const NodeID& peer_id, const ContentResponse& response) {
    // TODO: Integrate with SessionManager to actually send over network
    (void)peer_id;
    (void)response;
    CASHEW_LOG_DEBUG("Would send response to peer (network integration pending)");
}

// RouterStatistics methods

std::string RouterStatistics::to_string() const {
    std::ostringstream oss;
    oss << "Router Statistics:\n";
    oss << "  Requests sent: " << total_requests_sent << "\n";
    oss << "  Requests received: " << total_requests_received << "\n";
    oss << "  Responses sent: " << total_responses_sent << "\n";
    oss << "  Responses received: " << total_responses_received << "\n";
    oss << "  Forwards: " << total_forwards << "\n";
    oss << "  Successful retrievals: " << successful_retrievals << "\n";
    oss << "  Failed retrievals: " << failed_retrievals << "\n";
    oss << "  Average hop count: " << average_hop_count << "\n";
    oss << "  Routing table size: " << routing_table_size << "\n";
    oss << "  Pending requests: " << pending_requests_count;
    return oss.str();
}

} // namespace cashew::network
