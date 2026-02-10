#include "node_identity.hpp"
#include "../../crypto/ed25519.hpp"
#include "../../crypto/blake3.hpp"
#include "../../crypto/chacha20poly1305.hpp"
#include "../../utils/logger.hpp"
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>

namespace cashew::core {

using json = nlohmann::json;

NodeIdentity::NodeIdentity(const PublicKey& pk, const SecretKey& sk, uint64_t timestamp)
    : public_key_(pk)
    , secret_key_(sk)
    , created_timestamp_(timestamp)
    , rotation_history_()
{
    // Derive NodeID from public key
    auto hash = crypto::Blake3::hash(bytes(pk.begin(), pk.end()));
    node_id_ = NodeID(hash);
}

NodeIdentity::NodeIdentity(const PublicKey& pk, const SecretKey& sk, uint64_t timestamp,
                           const std::vector<RotationCertificate>& history)
    : public_key_(pk)
    , secret_key_(sk)
    , created_timestamp_(timestamp)
    , rotation_history_(history)
{
    // Derive NodeID from public key
    auto hash = crypto::Blake3::hash(bytes(pk.begin(), pk.end()));
    node_id_ = NodeID(hash);
}

NodeIdentity NodeIdentity::generate() {
    CASHEW_LOG_DEBUG("Generating new node identity");
    
    auto [pk, sk] = crypto::Ed25519::generate_keypair();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    NodeIdentity identity(pk, sk, timestamp);
    CASHEW_LOG_INFO("Generated node identity: {}", identity.id().to_string());
    
    return identity;
}

NodeIdentity NodeIdentity::load(const std::filesystem::path& path, const std::string& password) {
    CASHEW_LOG_DEBUG("Loading node identity from: {}", path.string());
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open identity file: " + path.string());
    }
    
    // Read file contents
    bytes file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    bytes json_data;
    
    // Decrypt if password provided
    if (!password.empty()) {
        // First 12 bytes are nonce, rest is encrypted data
        if (file_data.size() < 12) {
            throw std::runtime_error("Invalid encrypted identity file");
        }
        
        Nonce nonce;
        std::copy(file_data.begin(), file_data.begin() + 12, nonce.begin());
        bytes ciphertext(file_data.begin() + 12, file_data.end());
        
        // Derive key from password
        auto password_bytes = bytes(password.begin(), password.end());
        auto key_hash = crypto::Blake3::hash(password_bytes);
        SessionKey key;
        std::copy(key_hash.begin(), key_hash.end(), key.begin());
        
        auto decrypted = crypto::ChaCha20Poly1305::decrypt(ciphertext, key, nonce);
        if (!decrypted) {
            throw std::runtime_error("Failed to decrypt identity file (wrong password?)");
        }
        json_data = *decrypted;
    } else {
        json_data = file_data;
    }
    
    // Parse JSON
    std::string json_str(json_data.begin(), json_data.end());
    json j = json::parse(json_str);
    
    // Extract keys and timestamp
    auto pk_hex = j["public_key"].get<std::string>();
    auto sk_hex = j["secret_key"].get<std::string>();
    uint64_t timestamp = j["created_timestamp"].get<uint64_t>();
    
    auto pk = crypto::Ed25519::public_key_from_hex(pk_hex);
    if (!pk) {
        throw std::runtime_error("Invalid public key in identity file");
    }
    
    // Parse secret key (stored as hex)
    SecretKey sk;
    if (sk_hex.length() != sk.size() * 2) {
        throw std::runtime_error("Invalid secret key in identity file");
    }
    for (size_t i = 0; i < sk.size(); ++i) {
        sk[i] = static_cast<byte>(std::stoul(sk_hex.substr(i * 2, 2), nullptr, 16));
    }
    
    // Load rotation history if present
    std::vector<RotationCertificate> rotation_history;
    if (j.contains("rotation_history") && j["rotation_history"].is_array()) {
        for (const auto& cert_json : j["rotation_history"]) {
            RotationCertificate cert;
            
            auto old_pk_hex = cert_json["old_public_key"].get<std::string>();
            auto new_pk_hex = cert_json["new_public_key"].get<std::string>();
            auto sig_hex = cert_json["signature"].get<std::string>();
            
            auto old_pk = crypto::Ed25519::public_key_from_hex(old_pk_hex);
            auto new_pk = crypto::Ed25519::public_key_from_hex(new_pk_hex);
            
            if (!old_pk || !new_pk) {
                throw std::runtime_error("Invalid public key in rotation certificate");
            }
            
            cert.old_public_key = *old_pk;
            cert.new_public_key = *new_pk;
            cert.rotation_timestamp = cert_json["rotation_timestamp"].get<uint64_t>();
            
            if (cert_json.contains("reason")) {
                cert.reason = cert_json["reason"].get<std::string>();
            }
            
            // Parse signature
            if (sig_hex.length() != cert.old_key_signature.size() * 2) {
                throw std::runtime_error("Invalid signature in rotation certificate");
            }
            for (size_t i = 0; i < cert.old_key_signature.size(); ++i) {
                cert.old_key_signature[i] = static_cast<byte>(
                    std::stoul(sig_hex.substr(i * 2, 2), nullptr, 16)
                );
            }
            
            rotation_history.push_back(cert);
        }
    }
    
    CASHEW_LOG_INFO("Loaded node identity from file");
    return NodeIdentity(*pk, sk, timestamp, rotation_history);
}

void NodeIdentity::save(const std::filesystem::path& path, const std::string& password) const {
    CASHEW_LOG_DEBUG("Saving node identity to: {}", path.string());
    
    // Create directory if needed
    std::filesystem::create_directories(path.parent_path());
    
    // Convert secret key to hex
    std::string sk_hex;
    for (auto b : secret_key_) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", b);
        sk_hex += buf;
    }
    
    // Build JSON
    json j;
    j["public_key"] = crypto::Ed25519::public_key_to_hex(public_key_);
    j["secret_key"] = sk_hex;
    j["created_timestamp"] = created_timestamp_;
    j["node_id"] = node_id_.to_string();
    
    // Save rotation history
    if (!rotation_history_.empty()) {
        json rotation_array = json::array();
        for (const auto& cert : rotation_history_) {
            json cert_json;
            cert_json["old_public_key"] = crypto::Ed25519::public_key_to_hex(cert.old_public_key);
            cert_json["new_public_key"] = crypto::Ed25519::public_key_to_hex(cert.new_public_key);
            cert_json["rotation_timestamp"] = cert.rotation_timestamp;
            
            // Convert signature to hex
            std::string sig_hex;
            for (auto b : cert.old_key_signature) {
                char buf[3];
                snprintf(buf, sizeof(buf), "%02x", b);
                sig_hex += buf;
            }
            cert_json["signature"] = sig_hex;
            
            if (!cert.reason.empty()) {
                cert_json["reason"] = cert.reason;
            }
            
            rotation_array.push_back(cert_json);
        }
        j["rotation_history"] = rotation_array;
    }
    
    std::string json_str = j.dump();
    bytes json_data(json_str.begin(), json_str.end());
    
    bytes file_data;
    
    // Encrypt if password provided
    if (!password.empty()) {
        auto password_bytes = bytes(password.begin(), password.end());
        auto key_hash = crypto::Blake3::hash(password_bytes);
        SessionKey key;
        std::copy(key_hash.begin(), key_hash.end(), key.begin());
        
        auto nonce = crypto::ChaCha20Poly1305::generate_nonce();
        auto ciphertext = crypto::ChaCha20Poly1305::encrypt(json_data, key, nonce);
        
        // Prepend nonce to ciphertext
        file_data.insert(file_data.end(), nonce.begin(), nonce.end());
        file_data.insert(file_data.end(), ciphertext.begin(), ciphertext.end());
    } else {
        file_data = json_data;
    }
    
    // Write to file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    
    file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
    file.close();
    
    CASHEW_LOG_INFO("Saved node identity to file");
}

Signature NodeIdentity::sign(const bytes& message) const {
    return crypto::Ed25519::sign(message, secret_key_);
}

bool NodeIdentity::verify(const bytes& message, const Signature& signature) const {
    return crypto::Ed25519::verify(message, signature, public_key_);
}

// RotationCertificate implementation
bytes RotationCertificate::to_bytes() const {
    bytes result;
    
    // Add old public key
    result.insert(result.end(), old_public_key.begin(), old_public_key.end());
    
    // Add new public key
    result.insert(result.end(), new_public_key.begin(), new_public_key.end());
    
    // Add timestamp (8 bytes, little endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<byte>((rotation_timestamp >> (i * 8)) & 0xFF));
    }
    
    // Add reason if present
    if (!reason.empty()) {
        result.insert(result.end(), reason.begin(), reason.end());
    }
    
    return result;
}

bool RotationCertificate::verify() const {
    // Create message to verify: new_key || timestamp
    bytes message;
    message.insert(message.end(), new_public_key.begin(), new_public_key.end());
    
    // Add timestamp
    for (int i = 0; i < 8; ++i) {
        message.push_back(static_cast<byte>((rotation_timestamp >> (i * 8)) & 0xFF));
    }
    
    // Verify signature using old public key
    return crypto::Ed25519::verify(message, old_key_signature, old_public_key);
}

NodeIdentity NodeIdentity::rotate(const std::string& reason) const {
    CASHEW_LOG_INFO("Rotating node identity (reason: {})", reason.empty() ? "none" : reason);
    
    // Generate new keypair
    auto [new_pk, new_sk] = crypto::Ed25519::generate_keypair();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // Create rotation certificate
    RotationCertificate cert;
    cert.old_public_key = public_key_;
    cert.new_public_key = new_pk;
    cert.rotation_timestamp = timestamp;
    cert.reason = reason;
    
    // Sign new key with old key to prove continuity
    bytes message;
    message.insert(message.end(), new_pk.begin(), new_pk.end());
    
    // Add timestamp to message
    for (int i = 0; i < 8; ++i) {
        message.push_back(static_cast<byte>((timestamp >> (i * 8)) & 0xFF));
    }
    
    cert.old_key_signature = crypto::Ed25519::sign(message, secret_key_);
    
    // Verify the certificate we just created
    if (!cert.verify()) {
        throw std::runtime_error("Failed to create valid rotation certificate");
    }
    
    // Create new identity with updated rotation history
    auto new_rotation_history = rotation_history_;
    new_rotation_history.push_back(cert);
    
    NodeIdentity new_identity(new_pk, new_sk, timestamp, new_rotation_history);
    
    CASHEW_LOG_INFO("Created new rotated identity: {}", new_identity.id().to_string());
    CASHEW_LOG_INFO("Rotation chain length: {}", new_rotation_history.size());
    
    return new_identity;
}

bool NodeIdentity::verify_rotation_chain() const {
    if (rotation_history_.empty()) {
        return true; // No rotations, nothing to verify
    }
    
    // Verify each certificate in the chain
    for (size_t i = 0; i < rotation_history_.size(); ++i) {
        const auto& cert = rotation_history_[i];
        
        if (!cert.verify()) {
            CASHEW_LOG_ERROR("Invalid rotation certificate at index {}", i);
            return false;
        }
        
        // Verify chain continuity: each cert's new_key should match the next cert's old_key
        if (i < rotation_history_.size() - 1) {
            const auto& next_cert = rotation_history_[i + 1];
            if (cert.new_public_key != next_cert.old_public_key) {
                CASHEW_LOG_ERROR("Broken rotation chain at index {}", i);
                return false;
            }
        }
    }
    
    // Verify that the last certificate's new_key matches our current public key
    const auto& last_cert = rotation_history_.back();
    if (last_cert.new_public_key != public_key_) {
        CASHEW_LOG_ERROR("Current public key doesn't match rotation chain");
        return false;
    }
    
    CASHEW_LOG_DEBUG("Rotation chain verified successfully ({} rotations)", rotation_history_.size());
    return true;
}

PublicKey NodeIdentity::get_genesis_key() const {
    if (rotation_history_.empty()) {
        return public_key_; // No rotations, current key is genesis key
    }
    
    // Return the old key from the first rotation certificate
    return rotation_history_[0].old_public_key;
}

void NodeIdentity::add_rotation_certificate(const RotationCertificate& cert) {
    rotation_history_.push_back(cert);
}

} // namespace cashew::core
