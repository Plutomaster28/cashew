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
    
    CASHEW_LOG_INFO("Loaded node identity from file");
    return NodeIdentity(*pk, sk, timestamp);
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

NodeIdentity NodeIdentity::rotate() const {
    CASHEW_LOG_INFO("Rotating node identity");
    
    // Generate new keypair
    auto new_identity = NodeIdentity::generate();
    
    // TODO: Create rotation certificate signed by old key
    // For now just return new identity
    
    return new_identity;
}

} // namespace cashew::core
