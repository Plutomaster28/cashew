#include "security/onion_routing.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <random>
#include <cstring>

namespace cashew::security {

// OnionLayer methods

std::vector<uint8_t> OnionLayer::to_bytes() const {
    std::vector<uint8_t> data;
    
    // Ephemeral key (32 bytes)
    data.insert(data.end(), ephemeral_key.begin(), ephemeral_key.end());
    
    // Ciphertext length (4 bytes)
    uint32_t ciphertext_len = static_cast<uint32_t>(ciphertext.size());
    for (int i = 0; i < 4; i++) {
        data.push_back(static_cast<uint8_t>(ciphertext_len >> (i * 8)));
    }
    
    // Ciphertext
    data.insert(data.end(), ciphertext.begin(), ciphertext.end());
    
    // MAC (16 bytes)
    data.insert(data.end(), mac.begin(), mac.end());
    
    return data;
}

std::optional<OnionLayer> OnionLayer::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 52) {  // 32 + 4 + 0 + 16 minimum
        return std::nullopt;
    }
    
    OnionLayer layer;
    size_t offset = 0;
    
    // Ephemeral key
    std::copy(data.begin(), data.begin() + 32, layer.ephemeral_key.begin());
    offset += 32;
    
    // Ciphertext length
    uint32_t ciphertext_len = 0;
    for (int i = 0; i < 4; i++) {
        ciphertext_len |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    if (offset + ciphertext_len + 16 > data.size()) {
        return std::nullopt;
    }
    
    // Ciphertext
    layer.ciphertext.assign(data.begin() + offset, data.begin() + offset + ciphertext_len);
    offset += ciphertext_len;
    
    // MAC
    std::copy(data.begin() + offset, data.begin() + offset + 16, layer.mac.begin());
    
    return layer;
}

// OnionRouteBuilder methods

std::vector<OnionLayer> OnionRouteBuilder::build_layers(
    const std::vector<OnionHop>& route_hops,
    const std::vector<uint8_t>& payload
) {
    if (route_hops.empty()) {
        return {};
    }
    
    std::vector<OnionLayer> layers;
    layers.reserve(route_hops.size());
    
    // Build backwards (innermost first)
    std::vector<uint8_t> current_payload = payload;
    
    for (auto it = route_hops.rbegin(); it != route_hops.rend(); ++it) {
        const auto& hop = *it;
        
        // Generate ephemeral key pair for this layer
        auto [ephemeral_public, ephemeral_secret] = crypto::X25519::generate_keypair();
        
        // Derive shared secret with this hop's public key
        auto shared_secret = derive_shared_secret(hop.node_public_key, ephemeral_secret);
        
        // Encrypt current payload
        OnionLayer layer;
        layer.ephemeral_key = ephemeral_public;
        layer.ciphertext = encrypt_layer(current_payload, shared_secret, layer.mac);
        
        // This encrypted layer becomes the payload for the next layer
        current_payload = layer.to_bytes();
        
        layers.push_back(layer);
    }
    
    // Reverse to get outermost first
    std::reverse(layers.begin(), layers.end());
    
    CASHEW_LOG_DEBUG("Built {} onion layers", layers.size());
    return layers;
}

std::vector<OnionLayer> OnionRouteBuilder::build_layers_with_routing(
    const std::vector<OnionHop>& route_hops,
    const std::vector<uint8_t>& payload
) {
    if (route_hops.empty()) {
        return {};
    }
    
    std::vector<OnionLayer> layers;
    layers.reserve(route_hops.size());
    
    // Start with final payload
    std::vector<uint8_t> current_payload = payload;
    
    // Build backwards
    for (size_t i = route_hops.size(); i > 0; --i) {
        const auto& hop = route_hops[i - 1];
        
        // Prepend next hop address if not last
        if (i < route_hops.size()) {
            const auto& next_hop = route_hops[i];
            
            // Format: [next_hop_id (32 bytes)] + [payload]
            std::vector<uint8_t> routing_payload;
            routing_payload.insert(routing_payload.end(), 
                                  next_hop.node_id.id.begin(), 
                                  next_hop.node_id.id.end());
            routing_payload.insert(routing_payload.end(),
                                  current_payload.begin(),
                                  current_payload.end());
            
            current_payload = routing_payload;
        }
        
        // Generate ephemeral key pair
        auto [ephemeral_public, ephemeral_secret] = crypto::X25519::generate_keypair();
        
        // Derive shared secret
        auto shared_secret = derive_shared_secret(hop.node_public_key, ephemeral_secret);
        
        // Encrypt
        OnionLayer layer;
        layer.ephemeral_key = ephemeral_public;
        layer.ciphertext = encrypt_layer(current_payload, shared_secret, layer.mac);
        
        current_payload = layer.to_bytes();
        layers.push_back(layer);
    }
    
    std::reverse(layers.begin(), layers.end());
    
    CASHEW_LOG_DEBUG("Built {} onion layers with routing", layers.size());
    return layers;
}

std::array<uint8_t, 32> OnionRouteBuilder::derive_shared_secret(
    const PublicKey& their_public_key,
    const SecretKey& our_secret_key
) const {
    auto result = crypto::X25519::exchange(our_secret_key, their_public_key);
    if (!result) {
        return std::array<uint8_t, 32>{{}};
    }
    return *result;
}

std::vector<uint8_t> OnionRouteBuilder::encrypt_layer(
    const std::vector<uint8_t>& plaintext,
    const std::array<uint8_t, 32>& shared_secret,
    std::array<uint8_t, 16>& mac_out
) const {
    // Use ChaCha20-Poly1305 for AEAD encryption
    Nonce nonce{{0}};  // Zero nonce (ephemeral keys make this safe)
    
    auto ciphertext = crypto::ChaCha20Poly1305::encrypt(
        plaintext,
        shared_secret,
        nonce
    );
    
    // Last 16 bytes of ciphertext are the Poly1305 MAC
    if (ciphertext.size() >= 16) {
        std::copy(ciphertext.end() - 16, ciphertext.end(), mac_out.begin());
        ciphertext.resize(ciphertext.size() - 16);
    } else {
        std::fill(mac_out.begin(), mac_out.end(), 0);
    }
    
    return ciphertext;
}

// OnionLayerPeeler methods

OnionLayerPeeler::OnionLayerPeeler(const SecretKey& node_secret_key)
    : node_secret_key_(node_secret_key)
{
}

std::optional<std::vector<uint8_t>> OnionLayerPeeler::peel_layer(const OnionLayer& layer) {
    // Derive shared secret from ephemeral key
    auto shared_secret = derive_shared_secret(layer.ephemeral_key);
    
    // Decrypt
    return decrypt_layer(layer.ciphertext, shared_secret, layer.mac);
}

bool OnionLayerPeeler::peel_with_routing(
    const OnionLayer& layer,
    NodeID& next_hop_out,
    std::vector<OnionLayer>& remaining_layers_out,
    std::vector<uint8_t>& final_payload_out
) {
    auto decrypted_opt = peel_layer(layer);
    if (!decrypted_opt) {
        CASHEW_LOG_ERROR("Failed to decrypt onion layer");
        return false;
    }
    
    const auto& decrypted = *decrypted_opt;
    
    // Check if this contains routing info (at least 32 bytes for NodeID)
    if (decrypted.size() < 32) {
        // No routing info, we're the final destination
        final_payload_out = decrypted;
        return true;
    }
    
    // Extract next hop
    std::copy(decrypted.begin(), decrypted.begin() + 32, next_hop_out.id.begin());
    
    // Remaining data might be more layers or final payload
    std::vector<uint8_t> remaining_data(decrypted.begin() + 32, decrypted.end());
    
    // Try to parse as OnionLayer
    auto next_layer_opt = OnionLayer::from_bytes(remaining_data);
    if (next_layer_opt) {
        remaining_layers_out.push_back(*next_layer_opt);
        return false;  // We're a relay, not destination
    } else {
        // Not a layer, must be final payload
        final_payload_out = remaining_data;
        return true;  // We're the destination
    }
}

std::array<uint8_t, 32> OnionLayerPeeler::derive_shared_secret(
    const PublicKey& ephemeral_public_key
) const {
    auto result = crypto::X25519::exchange(node_secret_key_, ephemeral_public_key);
    if (!result) {
        return std::array<uint8_t, 32>{{}};
    }
    return *result;
}

std::optional<std::vector<uint8_t>> OnionLayerPeeler::decrypt_layer(
    const std::vector<uint8_t>& ciphertext,
    const std::array<uint8_t, 32>& shared_secret,
    const std::array<uint8_t, 16>& mac
) const {
    // Reconstruct full ciphertext with MAC for decryption
    std::vector<uint8_t> full_ciphertext = ciphertext;
    full_ciphertext.insert(full_ciphertext.end(), mac.begin(), mac.end());
    
    Nonce nonce{{0}};
    
    auto plaintext_opt = crypto::ChaCha20Poly1305::decrypt(
        full_ciphertext,
        shared_secret,
        nonce
    );
    
    if (!plaintext_opt) {
        CASHEW_LOG_WARN("Onion layer decryption failed (MAC verification)");
    }
    
    return plaintext_opt;
}

// OnionCircuitManager methods

OnionCircuitManager::OnionCircuitManager(const NodeID& local_node_id, const SecretKey& secret_key)
    : local_node_id_(local_node_id),
      node_secret_key_(secret_key),
      builder_(),
      peeler_(secret_key),
      circuits_built_(0),
      layers_peeled_(0)
{
    CASHEW_LOG_INFO("OnionCircuitManager initialized");
}

std::vector<OnionHop> OnionCircuitManager::select_path(
    const std::vector<OnionHop>& available_nodes,
    const NodeID& destination,
    size_t path_length
) {
    std::vector<OnionHop> path;
    
    if (available_nodes.empty() || path_length == 0) {
        return path;
    }
    
    // Filter out local node and destination
    std::vector<OnionHop> candidates;
    for (const auto& node : available_nodes) {
        if (node.node_id != local_node_id_ && node.node_id != destination) {
            candidates.push_back(node);
        }
    }
    
    if (candidates.empty()) {
        return path;
    }
    
    // Randomly select path_length nodes
    size_t actual_length = std::min(path_length, candidates.size());
    
    for (size_t i = 0; i < actual_length; ++i) {
        auto node = select_random_node(candidates);
        path.push_back(node);
        
        // Remove selected node from candidates to avoid reuse
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [&node](const OnionHop& n) { return n.node_id == node.node_id; }),
            candidates.end()
        );
        
        if (candidates.empty()) {
            break;
        }
    }
    
    CASHEW_LOG_DEBUG("Selected onion path with {} hops", path.size());
    return path;
}

std::vector<OnionLayer> OnionCircuitManager::build_circuit(
    const OnionHop& destination,
    const std::vector<OnionHop>& available_nodes,
    const std::vector<uint8_t>& payload
) {
    // Select path (default 3 hops)
    auto path = select_path(available_nodes, destination.node_id, 3);
    
    // Add destination as final hop
    path.push_back(destination);
    
    // Build onion layers
    auto layers = builder_.build_layers_with_routing(path, payload);
    
    circuits_built_++;
    
    return layers;
}

bool OnionCircuitManager::process_onion_request(
    const std::vector<OnionLayer>& layers,
    NodeID& next_hop_out,
    std::vector<OnionLayer>& remaining_layers_out,
    std::vector<uint8_t>& final_payload_out
) {
    if (layers.empty()) {
        CASHEW_LOG_ERROR("Empty onion layers received");
        return false;
    }
    
    // Peel the first layer
    bool is_destination = peeler_.peel_with_routing(
        layers[0],
        next_hop_out,
        remaining_layers_out,
        final_payload_out
    );
    
    layers_peeled_++;
    
    if (is_destination) {
        CASHEW_LOG_DEBUG("We are the final destination");
    } else {
        CASHEW_LOG_DEBUG("Relaying to next hop");
    }
    
    return is_destination;
}

bool OnionCircuitManager::is_node_suitable_for_path(
    const OnionHop& node,
    const std::vector<OnionHop>& path
) const {
    // Don't reuse nodes in path
    for (const auto& hop : path) {
        if (hop.node_id == node.node_id) {
            return false;
        }
    }
    
    // TODO: Add more criteria:
    // - Geographic diversity (avoid same country/AS)
    // - Bandwidth requirements
    // - Reputation score
    // - Uptime history
    
    return true;
}

OnionHop OnionCircuitManager::select_random_node(const std::vector<OnionHop>& candidates) const {
    if (candidates.empty()) {
        return OnionHop();
    }
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, candidates.size() - 1);
    
    return candidates[dis(gen)];
}

} // namespace cashew::security
