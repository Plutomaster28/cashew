#pragma once

#include "cashew/common.hpp"
#include "crypto/x25519.hpp"
#include "crypto/chacha20poly1305.hpp"
#include <chrono>
#include <optional>
#include <memory>

namespace cashew::network {

/**
 * SessionState - Current state of a P2P session
 */
enum class SessionState : uint8_t {
    DISCONNECTED = 0,
    HANDSHAKE_INIT = 1,      // Initiator sent handshake
    HANDSHAKE_RESPONSE = 2,  // Responder replied
    ESTABLISHED = 3,          // Keys derived, session active
    CLOSING = 4,              // Graceful shutdown in progress
    CLOSED = 5                // Session terminated
};

/**
 * SessionKeys - Derived keys for bidirectional encrypted communication
 */
struct SessionKeys {
    SessionKey tx_key;  // For sending (our direction)
    SessionKey rx_key;  // For receiving (their direction)
    uint64_t created_timestamp;
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    
    SessionKeys() 
        : created_timestamp(0), messages_sent(0), messages_received(0),
          bytes_sent(0), bytes_received(0) {}
};

/**
 * HandshakeMessage - Initial connection establishment
 */
struct HandshakeMessage {
    uint8_t version;                              // Protocol version (1)
    PublicKey ephemeral_public;     // Ephemeral DH key
    NodeID node_id;                               // Sender's node ID
    uint64_t timestamp;                           // Timestamp
    Signature signature;                          // Ed25519 signature
    
    static constexpr uint8_t CURRENT_VERSION = 1;
    static constexpr size_t MAX_AGE_SECONDS = 60;  // Reject old handshakes
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<HandshakeMessage> from_bytes(const std::vector<uint8_t>& data);
};

/**
 * Session - Encrypted P2P connection between two nodes
 * 
 * Key design principles:
 * - Outbound-only connections (no listening sockets)
 * - Ephemeral session keys (destroyed on close)
 * - Forward secrecy (old sessions not recoverable)
 * - Short-lived (rekey after 1 hour or 1GB)
 * - ChaCha20-Poly1305 for encryption
 * - X25519 for key exchange
 */
class Session {
public:
    Session(const NodeID& local_node_id, const NodeID& remote_node_id);
    ~Session();
    
    // Session lifecycle
    bool initiate_handshake();  // Initiator: start connection
    bool handle_handshake_response(const HandshakeMessage& response);  // Initiator: process response
    
    bool handle_handshake_init(const HandshakeMessage& init);  // Responder: receive handshake
    bool send_handshake_response();  // Responder: send response
    
    void close();
    
    // State
    SessionState get_state() const { return state_; }
    bool is_established() const { return state_ == SessionState::ESTABLISHED; }
    bool is_initiator() const { return is_initiator_; }
    
    // Identity
    NodeID get_local_node_id() const { return local_node_id_; }
    NodeID get_remote_node_id() const { return remote_node_id_; }
    
    // Encrypted messaging
    std::optional<std::vector<uint8_t>> encrypt_message(const std::vector<uint8_t>& plaintext);
    std::optional<std::vector<uint8_t>> decrypt_message(const std::vector<uint8_t>& ciphertext);
    
    // Session health
    bool should_rekey() const;  // Check if session needs rekeying
    bool has_timed_out() const;  // Check idle timeout
    
    // Statistics
    uint64_t get_messages_sent() const { return keys_.messages_sent; }
    uint64_t get_messages_received() const { return keys_.messages_received; }
    uint64_t get_bytes_sent() const { return keys_.bytes_sent; }
    uint64_t get_bytes_received() const { return keys_.bytes_received; }
    uint64_t get_age_seconds() const;
    uint64_t get_idle_seconds() const;
    
    // For testing/debugging
    HandshakeMessage get_last_handshake() const { return last_handshake_; }

private:
    NodeID local_node_id_;
    NodeID remote_node_id_;
    
    SessionState state_;
    bool is_initiator_;
    
    // Ephemeral key pair (destroyed on session close)
    std::pair<PublicKey, SecretKey> local_ephemeral_;
    PublicKey remote_ephemeral_;
    
    // Derived session keys
    SessionKeys keys_;
    
    // Handshake tracking
    HandshakeMessage last_handshake_;
    
    // Timestamps
    uint64_t created_timestamp_;
    uint64_t established_timestamp_;
    uint64_t last_activity_timestamp_;
    
    // Session limits
    static constexpr uint64_t IDLE_TIMEOUT_SECONDS = 1800;  // 30 minutes
    static constexpr uint64_t REKEY_INTERVAL_SECONDS = 3600;  // 1 hour
    static constexpr uint64_t REKEY_BYTES_LIMIT = 1ULL * 1024 * 1024 * 1024;  // 1 GB
    
    // Key derivation
    bool derive_session_keys(const SessionKey& shared_secret);
    
    // Nonce generation (must be unique per message)
    std::vector<uint8_t> generate_nonce();
    
    uint64_t nonce_counter_;
};

/**
 * SessionManager - Manages all active sessions for a node
 */
class SessionManager {
public:
    SessionManager(const NodeID& local_node_id);
    ~SessionManager() = default;
    
    // Session creation
    std::shared_ptr<Session> create_outbound_session(const NodeID& remote_node_id);
    std::shared_ptr<Session> handle_inbound_handshake(const HandshakeMessage& handshake);
    
    // Session lookup
    std::shared_ptr<Session> get_session(const NodeID& remote_node_id) const;
    bool has_session(const NodeID& remote_node_id) const;
    
    // Session management
    void close_session(const NodeID& remote_node_id);
    void close_all_sessions();
    
    // Maintenance
    void cleanup_stale_sessions();  // Remove timed-out sessions
    void rekey_old_sessions();       // Rekey sessions that need it
    
    // Statistics
    size_t active_session_count() const;
    std::vector<NodeID> get_connected_peers() const;
    
private:
    NodeID local_node_id_;
    std::vector<std::shared_ptr<Session>> sessions_;
    
    void remove_session(const NodeID& remote_node_id);
};

} // namespace cashew::network
