#pragma once

#include "cashew/common.hpp"
#include "crypto/x25519.hpp"
#include "crypto/chacha20poly1305.hpp"
#include "crypto/blake3.hpp"
#include <vector>
#include <optional>
#include <map>

namespace cashew::security {

/**
 * OnionLayer - Single layer of onion encryption
 * 
 * Structure:
 * - Ephemeral public key (32 bytes)
 * - Encrypted payload (variable)
 * - MAC tag (16 bytes)
 */
struct OnionLayer {
    PublicKey ephemeral_key;          // X25519 public key for this layer
    std::vector<uint8_t> ciphertext;  // Encrypted data
    std::array<uint8_t, 16> mac;      // Authentication tag
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<OnionLayer> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * OnionHop - Information about one hop in the route
 */
struct OnionHop {
    NodeID node_id;
    PublicKey node_public_key;  // X25519 key for encryption
    
    OnionHop() = default;
    OnionHop(const NodeID& id, const PublicKey& key) 
        : node_id(id), node_public_key(key) {}
};

/**
 * OnionRouteBuilder - Creates multi-layer encrypted onion routes
 * 
 * Design:
 * - Each layer encrypted with X25519-derived shared secret
 * - ChaCha20-Poly1305 for encryption
 * - Backwards construction (innermost first)
 * - Forward secrecy via ephemeral keys
 * 
 * Security properties:
 * - Each hop only sees: previous hop, next hop
 * - Content only visible at final destination
 * - No hop knows full route path
 * - Ephemeral keys prevent traffic correlation
 */
class OnionRouteBuilder {
public:
    OnionRouteBuilder() = default;
    ~OnionRouteBuilder() = default;
    
    /**
     * Build encrypted onion layers for a route
     * 
     * @param route_hops Path of nodes to traverse (first = entry, last = exit)
     * @param payload Final payload to deliver
     * @return Encrypted layers (first = outermost)
     */
    std::vector<OnionLayer> build_layers(
        const std::vector<OnionHop>& route_hops,
        const std::vector<uint8_t>& payload
    );
    
    /**
     * Build layers with routing instructions
     * Each hop receives encrypted next-hop address + remaining layers
     */
    std::vector<OnionLayer> build_layers_with_routing(
        const std::vector<OnionHop>& route_hops,
        const std::vector<uint8_t>& payload
    );
    
private:
    // Crypto helpers
    std::array<uint8_t, 32> derive_shared_secret(
        const PublicKey& their_public_key,
        const SecretKey& our_secret_key
    ) const;
    
    std::vector<uint8_t> encrypt_layer(
        const std::vector<uint8_t>& plaintext,
        const std::array<uint8_t, 32>& shared_secret,
        std::array<uint8_t, 16>& mac_out
    ) const;
};

/**
 * OnionLayerPeeler - Decrypts one layer of onion encryption
 * 
 * Each node:
 * 1. Receives encrypted layers
 * 2. Peels outermost layer using its private key
 * 3. Forwards remaining layers to next hop
 * 4. Doesn't know full route or final payload
 */
class OnionLayerPeeler {
public:
    explicit OnionLayerPeeler(const SecretKey& node_secret_key);
    ~OnionLayerPeeler() = default;
    
    /**
     * Decrypt the outermost onion layer
     * 
     * @param layer Encrypted layer to peel
     * @return Decrypted payload (may contain next hop info + remaining layers)
     */
    std::optional<std::vector<uint8_t>> peel_layer(const OnionLayer& layer);
    
    /**
     * Peel layer and extract routing information
     * 
     * @param layer Layer to decrypt
     * @param next_hop_out NodeID of next hop in route
     * @param remaining_layers_out Remaining encrypted layers
     * @param final_payload_out Final payload if we're destination
     * @return true if successfully peeled and extracted routing info
     */
    bool peel_with_routing(
        const OnionLayer& layer,
        NodeID& next_hop_out,
        std::vector<OnionLayer>& remaining_layers_out,
        std::vector<uint8_t>& final_payload_out
    );
    
private:
    SecretKey node_secret_key_;
    
    std::array<uint8_t, 32> derive_shared_secret(
        const PublicKey& ephemeral_public_key
    ) const;
    
    std::optional<std::vector<uint8_t>> decrypt_layer(
        const std::vector<uint8_t>& ciphertext,
        const std::array<uint8_t, 32>& shared_secret,
        const std::array<uint8_t, 16>& mac
    ) const;
};

/**
 * OnionCircuitManager - Manages onion-routed circuits
 * 
 * Responsibilities:
 * - Select random paths through network
 * - Build circuits with encrypted layers
 * - Maintain circuit state
 * - Rotate circuits periodically
 * 
 * Security features:
 * - Random path selection
 * - Avoid node reuse in single path
 * - Circuit rotation every N minutes
 * - Geographic/AS diversity (TODO)
 */
class OnionCircuitManager {
public:
    OnionCircuitManager(const NodeID& local_node_id, const SecretKey& secret_key);
    ~OnionCircuitManager() = default;
    
    /**
     * Select a random path through the network
     * 
     * @param available_nodes Nodes that can be used as hops
     * @param destination Final destination node
     * @param path_length Number of hops (typically 3-5)
     * @return Selected path (excludes destination)
     */
    std::vector<OnionHop> select_path(
        const std::vector<OnionHop>& available_nodes,
        const NodeID& destination,
        size_t path_length
    );
    
    /**
     * Build circuit for sending to destination
     * 
     * @param destination Final destination node + key
     * @param available_nodes Available relay nodes
     * @param payload Data to send
     * @return Encrypted onion layers
     */
    std::vector<OnionLayer> build_circuit(
        const OnionHop& destination,
        const std::vector<OnionHop>& available_nodes,
        const std::vector<uint8_t>& payload
    );
    
    /**
     * Process received onion request (relay or final destination)
     * 
     * @param layers Encrypted layers
     * @param next_hop_out Next hop if relaying
     * @param final_payload_out Payload if we're destination
     * @return true if we're destination, false if relaying
     */
    bool process_onion_request(
        const std::vector<OnionLayer>& layers,
        NodeID& next_hop_out,
        std::vector<OnionLayer>& remaining_layers_out,
        std::vector<uint8_t>& final_payload_out
    );
    
    // Statistics
    uint64_t get_circuits_built() const { return circuits_built_; }
    uint64_t get_layers_peeled() const { return layers_peeled_; }
    
private:
    NodeID local_node_id_;
    SecretKey node_secret_key_;
    OnionRouteBuilder builder_;
    OnionLayerPeeler peeler_;
    
    // Statistics
    uint64_t circuits_built_;
    uint64_t layers_peeled_;
    
    // Helpers
    bool is_node_suitable_for_path(const OnionHop& node, const std::vector<OnionHop>& path) const;
    OnionHop select_random_node(const std::vector<OnionHop>& candidates) const;
};

} // namespace cashew::security
