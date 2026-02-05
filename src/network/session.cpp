#include "session.hpp"
#include "utils/logger.hpp"
#include "crypto/random.hpp"
#include "crypto/blake3.hpp"
#include <algorithm>
#include <cstring>

namespace cashew::network {

// HandshakeMessage implementation

std::vector<uint8_t> HandshakeMessage::to_bytes() const {
    std::vector<uint8_t> data;
    data.reserve(1 + 32 + 32 + 8 + 64);
    
    // Version
    data.push_back(version);
    
    // Ephemeral public key
    data.insert(data.end(), 
               ephemeral_public.begin(), 
               ephemeral_public.end());
    
    // Node ID
    data.insert(data.end(), 
               node_id.id.begin(), 
               node_id.id.end());
    
    // Timestamp
    for (int i = 0; i < 8; ++i) {
        data.push_back((timestamp >> (i * 8)) & 0xFF);
    }
    
    // Signature
    data.insert(data.end(), 
               signature.begin(), 
               signature.end());
    
    return data;
}

std::optional<HandshakeMessage> HandshakeMessage::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 1 + 32 + 32 + 8 + 64) {
        return std::nullopt;
    }
    
    HandshakeMessage msg;
    size_t offset = 0;
    
    // Version
    msg.version = data[offset++];
    
    // Ephemeral public key
    std::copy(data.begin() + offset, 
             data.begin() + offset + 32,
             msg.ephemeral_public.begin());
    offset += 32;
    
    // Node ID
    std::copy(data.begin() + offset,
             data.begin() + offset + 32,
             msg.node_id.id.begin());
    offset += 32;
    
    // Timestamp
    msg.timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        msg.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Signature
    std::copy(data.begin() + offset,
             data.begin() + offset + 64,
             msg.signature.begin());
    
    return msg;
}

// Session implementation

Session::Session(const NodeID& local_node_id, const NodeID& remote_node_id)
    : local_node_id_(local_node_id),
      remote_node_id_(remote_node_id),
      state_(SessionState::DISCONNECTED),
      is_initiator_(false),
      nonce_counter_(0) {
    
    auto now = std::chrono::system_clock::now();
    created_timestamp_ = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    established_timestamp_ = 0;
    last_activity_timestamp_ = created_timestamp_;
    
    CASHEW_LOG_DEBUG("Created session with peer {}",
                    crypto::Blake3::hash_to_hex(remote_node_id.id));
}

Session::~Session() {
    close();
}

bool Session::initiate_handshake() {
    if (state_ != SessionState::DISCONNECTED) {
        CASHEW_LOG_WARN("Cannot initiate handshake in state {}", static_cast<int>(state_));
        return false;
    }
    
    is_initiator_ = true;
    
    // Generate ephemeral key pair
    local_ephemeral_ = crypto::X25519::generate_keypair();
    
    // Create handshake message
    HandshakeMessage handshake;
    handshake.version = HandshakeMessage::CURRENT_VERSION;
    handshake.ephemeral_public = local_ephemeral_.first;
    handshake.node_id = local_node_id_;
    
    auto now = std::chrono::system_clock::now();
    handshake.timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    // TODO: Sign handshake with node's identity key
    handshake.signature = Signature{};
    
    last_handshake_ = handshake;
    state_ = SessionState::HANDSHAKE_INIT;
    
    CASHEW_LOG_INFO("Initiated handshake with {}",
                   crypto::Blake3::hash_to_hex(remote_node_id_.id));
    
    return true;
}

bool Session::handle_handshake_response(const HandshakeMessage& response) {
    if (state_ != SessionState::HANDSHAKE_INIT) {
        CASHEW_LOG_WARN("Unexpected handshake response in state {}", static_cast<int>(state_));
        return false;
    }
    
    // Verify timestamp freshness
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    if (now_seconds - response.timestamp > HandshakeMessage::MAX_AGE_SECONDS) {
        CASHEW_LOG_WARN("Handshake response too old");
        return false;
    }
    
    // TODO: Verify signature with remote node's public key
    
    // Store remote ephemeral key
    remote_ephemeral_ = response.ephemeral_public;
    
    // Perform X25519 key exchange
    auto shared_secret_opt = crypto::X25519::exchange(
        local_ephemeral_.second,
        remote_ephemeral_
    );
    
    if (!shared_secret_opt) {
        CASHEW_LOG_ERROR("X25519 key exchange failed");
        state_ = SessionState::DISCONNECTED;
        return false;
    }
    
    // Derive session keys
    if (!derive_session_keys(*shared_secret_opt)) {
        CASHEW_LOG_ERROR("Failed to derive session keys");
        state_ = SessionState::DISCONNECTED;
        return false;
    }
    
    state_ = SessionState::ESTABLISHED;
    established_timestamp_ = now_seconds;
    last_activity_timestamp_ = now_seconds;
    
    CASHEW_LOG_INFO("Session established with {}",
                   crypto::Blake3::hash_to_hex(remote_node_id_.id));
    
    return true;
}

bool Session::handle_handshake_init(const HandshakeMessage& init) {
    if (state_ != SessionState::DISCONNECTED) {
        CASHEW_LOG_WARN("Cannot handle handshake in state {}", static_cast<int>(state_));
        return false;
    }
    
    is_initiator_ = false;
    
    // Verify timestamp freshness
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    if (now_seconds - init.timestamp > HandshakeMessage::MAX_AGE_SECONDS) {
        CASHEW_LOG_WARN("Handshake init too old");
        return false;
    }
    
    // TODO: Verify signature with remote node's public key
    
    // Store remote ephemeral key
    remote_ephemeral_ = init.ephemeral_public;
    
    // Generate our ephemeral key pair
    local_ephemeral_ = crypto::X25519::generate_keypair();
    
    state_ = SessionState::HANDSHAKE_RESPONSE;
    
    CASHEW_LOG_INFO("Received handshake from {}",
                   crypto::Blake3::hash_to_hex(init.node_id.id));
    
    return true;
}

bool Session::send_handshake_response() {
    if (state_ != SessionState::HANDSHAKE_RESPONSE) {
        CASHEW_LOG_WARN("Cannot send response in state {}", static_cast<int>(state_));
        return false;
    }
    
    // Create response message
    HandshakeMessage response;
    response.version = HandshakeMessage::CURRENT_VERSION;
    response.ephemeral_public = local_ephemeral_.first;
    response.node_id = local_node_id_;
    
    auto now = std::chrono::system_clock::now();
    response.timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    // TODO: Sign response with node's identity key
    response.signature = Signature{};
    
    // Perform X25519 key exchange
    auto shared_secret = crypto::X25519::exchange(
        local_ephemeral_.second,
        remote_ephemeral_
    );
    
    if (!shared_secret) {
        CASHEW_LOG_ERROR("X25519 key exchange failed");
        state_ = SessionState::DISCONNECTED;
        return false;
    }
    
    // Derive session keys
    if (!derive_session_keys(*shared_secret)) {
        CASHEW_LOG_ERROR("Failed to derive session keys");
        state_ = SessionState::DISCONNECTED;
        return false;
    }
    
    state_ = SessionState::ESTABLISHED;
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    established_timestamp_ = now_seconds;
    last_activity_timestamp_ = now_seconds;
    
    last_handshake_ = response;
    
    CASHEW_LOG_INFO("Sent handshake response, session established");
    
    return true;
}

void Session::close() {
    if (state_ == SessionState::CLOSED) {
        return;
    }
    
    // Zero out ephemeral keys for forward secrecy
    std::fill(local_ephemeral_.second.begin(), 
             local_ephemeral_.second.end(), 0);
    std::fill(local_ephemeral_.first.begin(),
             local_ephemeral_.first.end(), 0);
    
    // Zero out session keys
    std::fill(keys_.tx_key.begin(), keys_.tx_key.end(), 0);
    std::fill(keys_.rx_key.begin(), keys_.rx_key.end(), 0);
    
    state_ = SessionState::CLOSED;
    
    CASHEW_LOG_INFO("Session closed with {}",
                   crypto::Blake3::hash_to_hex(remote_node_id_.id));
}

std::optional<std::vector<uint8_t>> Session::encrypt_message(const std::vector<uint8_t>& plaintext) {
    if (state_ != SessionState::ESTABLISHED) {
        return std::nullopt;
    }
    
    // Generate unique nonce
    auto nonce = generate_nonce();
    
    // Convert nonce vector to Nonce type
    Nonce nonce_array;
    std::copy(nonce.begin(), nonce.end(), nonce_array.begin());
    
    // Encrypt with ChaCha20-Poly1305
    auto ciphertext = crypto::ChaCha20Poly1305::encrypt(plaintext, keys_.tx_key, nonce_array);
    
    // Update statistics
    keys_.messages_sent++;
    keys_.bytes_sent += plaintext.size();
    
    auto now = std::chrono::system_clock::now();
    last_activity_timestamp_ = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    return ciphertext;
}

std::optional<std::vector<uint8_t>> Session::decrypt_message(const std::vector<uint8_t>& ciphertext) {
    if (state_ != SessionState::ESTABLISHED) {
        return std::nullopt;
    }
    
    // TODO: Extract nonce from ciphertext and decrypt properly with ChaCha20Poly1305
    // For now, return nullopt as placeholder
    (void)ciphertext;
    return std::nullopt;
}

bool Session::should_rekey() const {
    if (state_ != SessionState::ESTABLISHED) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    // Check time limit
    uint64_t age = now_seconds - established_timestamp_;
    if (age >= REKEY_INTERVAL_SECONDS) {
        return true;
    }
    
    // Check byte limit
    uint64_t total_bytes = keys_.bytes_sent + keys_.bytes_received;
    if (total_bytes >= REKEY_BYTES_LIMIT) {
        return true;
    }
    
    return false;
}

bool Session::has_timed_out() const {
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    uint64_t idle = now_seconds - last_activity_timestamp_;
    return idle >= IDLE_TIMEOUT_SECONDS;
}

uint64_t Session::get_age_seconds() const {
    if (established_timestamp_ == 0) {
        return 0;
    }
    
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    return now_seconds - established_timestamp_;
}

uint64_t Session::get_idle_seconds() const {
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    return now_seconds - last_activity_timestamp_;
}

bool Session::derive_session_keys(const SessionKey& shared_secret) {
    // Use HKDF-like derivation with BLAKE3
    std::vector<uint8_t> key_material;
    key_material.insert(key_material.end(), 
                       shared_secret.begin(), 
                       shared_secret.end());
    
    // Add context string
    std::string context = "cashew_session_v1";
    key_material.insert(key_material.end(), context.begin(), context.end());
    
    // Hash to get 64 bytes (32 for tx, 32 for rx)
    auto derived = crypto::Blake3::hash(key_material);
    
    // Split into tx and rx keys
    // Initiator: first half is tx, second half is rx
    // Responder: first half is rx, second half is tx
    
    if (is_initiator_) {
        std::copy(derived.begin(), derived.begin() + 32,
                 keys_.tx_key.begin());
        
        // For rx key, hash again with different context
        key_material.push_back(1);
        auto derived2 = crypto::Blake3::hash(key_material);
        std::copy(derived2.begin(), derived2.begin() + 32,
                 keys_.rx_key.begin());
    } else {
        std::copy(derived.begin(), derived.begin() + 32,
                 keys_.rx_key.begin());
        
        key_material.push_back(1);
        auto derived2 = crypto::Blake3::hash(key_material);
        std::copy(derived2.begin(), derived2.begin() + 32,
                 keys_.tx_key.begin());
    }
    
    auto now = std::chrono::system_clock::now();
    keys_.created_timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    return true;
}

std::vector<uint8_t> Session::generate_nonce() {
    // Create 12-byte nonce from counter + random
    std::vector<uint8_t> nonce(12);
    
    // First 8 bytes: counter
    for (int i = 0; i < 8; ++i) {
        nonce[i] = (nonce_counter_ >> (i * 8)) & 0xFF;
    }
    
    // Last 4 bytes: random
    auto random_bytes = crypto::Random::generate(4);
    std::copy(random_bytes.begin(), random_bytes.end(), nonce.begin() + 8);
    
    nonce_counter_++;
    
    return nonce;
}

// SessionManager implementation

SessionManager::SessionManager(const NodeID& local_node_id)
    : local_node_id_(local_node_id) {
    CASHEW_LOG_INFO("Created session manager for node {}",
                   crypto::Blake3::hash_to_hex(local_node_id.id));
}

std::shared_ptr<Session> SessionManager::create_outbound_session(const NodeID& remote_node_id) {
    // Check if session already exists
    if (has_session(remote_node_id)) {
        return get_session(remote_node_id);
    }
    
    auto session = std::make_shared<Session>(local_node_id_, remote_node_id);
    sessions_.push_back(session);
    
    CASHEW_LOG_INFO("Created outbound session (total: {})", sessions_.size());
    
    return session;
}

std::shared_ptr<Session> SessionManager::handle_inbound_handshake(const HandshakeMessage& handshake) {
    // Check if session already exists
    if (has_session(handshake.node_id)) {
        auto existing = get_session(handshake.node_id);
        // TODO: Decide whether to replace or reject
        return existing;
    }
    
    auto session = std::make_shared<Session>(local_node_id_, handshake.node_id);
    
    if (!session->handle_handshake_init(handshake)) {
        return nullptr;
    }
    
    sessions_.push_back(session);
    
    CASHEW_LOG_INFO("Created inbound session (total: {})", sessions_.size());
    
    return session;
}

std::shared_ptr<Session> SessionManager::get_session(const NodeID& remote_node_id) const {
    for (const auto& session : sessions_) {
        if (session->get_remote_node_id() == remote_node_id) {
            return session;
        }
    }
    return nullptr;
}

bool SessionManager::has_session(const NodeID& remote_node_id) const {
    return get_session(remote_node_id) != nullptr;
}

void SessionManager::close_session(const NodeID& remote_node_id) {
    auto session = get_session(remote_node_id);
    if (session) {
        session->close();
        remove_session(remote_node_id);
    }
}

void SessionManager::close_all_sessions() {
    for (auto& session : sessions_) {
        session->close();
    }
    sessions_.clear();
    
    CASHEW_LOG_INFO("Closed all sessions");
}

void SessionManager::cleanup_stale_sessions() {
    size_t removed = 0;
    
    auto it = std::remove_if(sessions_.begin(), sessions_.end(),
        [&removed](const std::shared_ptr<Session>& session) {
            if (session->has_timed_out()) {
                session->close();
                ++removed;
                return true;
            }
            return false;
        });
    
    sessions_.erase(it, sessions_.end());
    
    if (removed > 0) {
        CASHEW_LOG_INFO("Cleaned up {} stale sessions", removed);
    }
}

void SessionManager::rekey_old_sessions() {
    for (const auto& session : sessions_) {
        if (session->should_rekey()) {
            CASHEW_LOG_INFO("Session with {} needs rekeying",
                           crypto::Blake3::hash_to_hex(session->get_remote_node_id().id));
            // TODO: Implement rekeying protocol
        }
    }
}

size_t SessionManager::active_session_count() const {
    size_t count = 0;
    for (const auto& session : sessions_) {
        if (session->is_established()) {
            ++count;
        }
    }
    return count;
}

std::vector<NodeID> SessionManager::get_connected_peers() const {
    std::vector<NodeID> peers;
    for (const auto& session : sessions_) {
        if (session->is_established()) {
            peers.push_back(session->get_remote_node_id());
        }
    }
    return peers;
}

void SessionManager::remove_session(const NodeID& remote_node_id) {
    auto it = std::remove_if(sessions_.begin(), sessions_.end(),
        [&remote_node_id](const std::shared_ptr<Session>& session) {
            return session->get_remote_node_id() == remote_node_id;
        });
    
    sessions_.erase(it, sessions_.end());
}

} // namespace cashew::network
