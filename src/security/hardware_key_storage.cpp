#include "security/hardware_key_storage.hpp"
#include "crypto/ed25519.hpp"
#include "utils/logger.hpp"
#include <cstring>

namespace cashew::security {

// SoftwareKeyStorage implementation

SoftwareKeyStorage::SoftwareKeyStorage()
    : initialized_(false)
{
}

HardwareKeyCapabilities SoftwareKeyStorage::get_capabilities() const {
    HardwareKeyCapabilities caps;
    caps.can_generate_keys = true;
    caps.can_sign = true;
    caps.can_encrypt = true;
    caps.can_export_public_key = true;
    caps.can_export_private_key = true;  // Software keys can be exported
    caps.requires_pin = false;
    caps.supports_key_attestation = false;  // No hardware attestation
    caps.max_key_size_bits = 256;
    return caps;
}

bool SoftwareKeyStorage::initialize(const std::string& /*pin*/) {
    initialized_ = true;
    CASHEW_LOG_INFO("Software key storage initialized");
    return true;
}

std::optional<HardwareKeyHandle> SoftwareKeyStorage::generate_key(const std::string& key_label) {
    if (!initialized_) {
        CASHEW_LOG_ERROR("Software key storage not initialized");
        return std::nullopt;
    }
    
    // Generate Ed25519 keypair
    auto [public_key, secret_key] = crypto::Ed25519::generate_keypair();
    
    // Create key ID from label
    std::string key_id = "sw_" + key_label;
    
    // Store in memory
    KeyEntry entry;
    entry.label = key_label;
    entry.public_key = public_key;
    entry.secret_key = secret_key;
    keys_[key_id] = entry;
    
    // Create handle
    HardwareKeyHandle handle;
    handle.type = HardwareKeyType::NONE;
    handle.device_identifier = "software";
    handle.key_identifier = key_id;
    
    CASHEW_LOG_INFO("Generated software key: {}", key_label);
    return handle;
}

std::optional<HardwareKeyHandle> SoftwareKeyStorage::import_key(
    const SecretKey& secret_key,
    const std::string& key_label
) {
    if (!initialized_) {
        CASHEW_LOG_ERROR("Software key storage not initialized");
        return std::nullopt;
    }
    
    // Derive public key from secret key
    auto public_key = crypto::Ed25519::secret_to_public(secret_key);
    
    // Create key ID
    std::string key_id = "sw_" + key_label;
    
    // Store
    KeyEntry entry;
    entry.label = key_label;
    entry.public_key = public_key;
    entry.secret_key = secret_key;
    keys_[key_id] = entry;
    
    // Create handle
    HardwareKeyHandle handle;
    handle.type = HardwareKeyType::NONE;
    handle.device_identifier = "software";
    handle.key_identifier = key_id;
    
    CASHEW_LOG_INFO("Imported software key: {}", key_label);
    return handle;
}

std::optional<PublicKey> SoftwareKeyStorage::get_public_key(const HardwareKeyHandle& handle) {
    auto it = keys_.find(handle.key_identifier);
    if (it == keys_.end()) {
        return std::nullopt;
    }
    return it->second.public_key;
}

std::optional<Signature> SoftwareKeyStorage::sign(
    const HardwareKeyHandle& handle,
    const bytes& data
) {
    auto it = keys_.find(handle.key_identifier);
    if (it == keys_.end()) {
        CASHEW_LOG_ERROR("Key not found: {}", handle.key_identifier);
        return std::nullopt;
    }
    
    // Sign using Ed25519
    return crypto::Ed25519::sign(data, it->second.secret_key);
}

bool SoftwareKeyStorage::delete_key(const HardwareKeyHandle& handle) {
    auto it = keys_.find(handle.key_identifier);
    if (it == keys_.end()) {
        return false;
    }
    
    // Securely clear secret key
    std::memset(it->second.secret_key.data(), 0, it->second.secret_key.size());
    
    keys_.erase(it);
    CASHEW_LOG_INFO("Deleted software key: {}", handle.key_identifier);
    return true;
}

std::optional<bytes> SoftwareKeyStorage::get_attestation(const HardwareKeyHandle& /*handle*/) {
    // Software keys have no attestation
    return std::nullopt;
}

std::vector<HardwareKeyHandle> SoftwareKeyStorage::list_keys() {
    std::vector<HardwareKeyHandle> handles;
    
    for (const auto& [key_id, entry] : keys_) {
        HardwareKeyHandle handle;
        handle.type = HardwareKeyType::NONE;
        handle.device_identifier = "software";
        handle.key_identifier = key_id;
        handles.push_back(handle);
    }
    
    return handles;
}

bool SoftwareKeyStorage::verify_attestation(const bytes& /*attestation*/) {
    // No attestation for software keys
    return false;
}

// TPMKeyStorage implementation (stub)

TPMKeyStorage::TPMKeyStorage()
    : tpm_available_(false)
    , initialized_(false)
{
    // In production: Check for TPM device
    // Linux: /dev/tpm0 or /dev/tpmrm0
    // Windows: TBS API
}

bool TPMKeyStorage::is_available() const {
    // Stub: Always return false
    // In production: Detect TPM 2.0 device
    return false;
}

HardwareKeyCapabilities TPMKeyStorage::get_capabilities() const {
    HardwareKeyCapabilities caps;
    
    if (!tpm_available_) {
        return caps;  // All false
    }
    
    // TPM 2.0 capabilities
    caps.can_generate_keys = true;
    caps.can_sign = true;
    caps.can_encrypt = true;
    caps.can_export_public_key = true;
    caps.can_export_private_key = false;  // Private keys stay in TPM
    caps.requires_pin = true;             // Usually requires authorization
    caps.supports_key_attestation = true; // TPM supports attestation
    caps.max_key_size_bits = 2048;        // TPM 2.0 RSA limit (Ed25519 is 256-bit)
    
    return caps;
}

bool TPMKeyStorage::initialize(const std::string& /*pin*/) {
    if (!tpm_available_) {
        CASHEW_LOG_WARN("TPM not available on this system");
        return false;
    }
    
    // Stub implementation
    // In production:
    // 1. Open TPM device
    // 2. Start auth session
    // 3. Set up hierarchy (owner/endorsement/platform)
    
    initialized_ = false;
    CASHEW_LOG_INFO("TPM support not compiled in (stub)");
    return false;
}

std::optional<HardwareKeyHandle> TPMKeyStorage::generate_key(const std::string& /*key_label*/) {
    if (!initialized_) {
        return std::nullopt;
    }
    
    // Stub: In production, use TPM2_Create to generate key
    return std::nullopt;
}

std::optional<HardwareKeyHandle> TPMKeyStorage::import_key(
    const SecretKey& /*secret_key*/,
    const std::string& /*key_label*/
) {
    // TPM can import keys using TPM2_Import (with duplication)
    return std::nullopt;
}

std::optional<PublicKey> TPMKeyStorage::get_public_key(const HardwareKeyHandle& /*handle*/) {
    // Use TPM2_ReadPublic
    return std::nullopt;
}

std::optional<Signature> TPMKeyStorage::sign(
    const HardwareKeyHandle& /*handle*/,
    const bytes& /*data*/
) {
    // Use TPM2_Sign
    return std::nullopt;
}

bool TPMKeyStorage::delete_key(const HardwareKeyHandle& /*handle*/) {
    // Use TPM2_EvictControl
    return false;
}

std::optional<bytes> TPMKeyStorage::get_attestation(const HardwareKeyHandle& /*handle*/) {
    // Use TPM2_Certify to create attestation
    return std::nullopt;
}

std::vector<HardwareKeyHandle> TPMKeyStorage::list_keys() {
    return {};
}

bool TPMKeyStorage::verify_attestation(const bytes& /*attestation*/) {
    // Verify TPM attestation signature
    return false;
}

// HardwareKeyStorageFactory implementation

std::unique_ptr<HardwareKeyStorage> HardwareKeyStorageFactory::create_best_available() {
    auto available = detect_available_hardware();
    
    // Priority order: TPM, others, software fallback
    if (std::find(available.begin(), available.end(), HardwareKeyType::TPM_2_0) != available.end()) {
        auto storage = create(HardwareKeyType::TPM_2_0);
        if (storage && storage->is_available()) {
            CASHEW_LOG_INFO("Using TPM 2.0 for key storage");
            return storage;
        }
    }
    
    // TODO: Add Secure Enclave, Windows NGC, YubiKey support
    
    // Fallback to software
    CASHEW_LOG_INFO("Using software key storage (no hardware security module detected)");
    return std::make_unique<SoftwareKeyStorage>();
}

std::unique_ptr<HardwareKeyStorage> HardwareKeyStorageFactory::create(HardwareKeyType type) {
    switch (type) {
        case HardwareKeyType::NONE:
            return std::make_unique<SoftwareKeyStorage>();
            
        case HardwareKeyType::TPM_2_0:
            return std::make_unique<TPMKeyStorage>();
            
        case HardwareKeyType::SECURE_ENCLAVE:
            // TODO: Implement SecureEnclaveKeyStorage for macOS/iOS
            CASHEW_LOG_WARN("Secure Enclave support not implemented");
            return nullptr;
            
        case HardwareKeyType::WINDOWS_NGC:
            // TODO: Implement WindowsNGCKeyStorage
            CASHEW_LOG_WARN("Windows NGC support not implemented");
            return nullptr;
            
        case HardwareKeyType::YUBIKEY:
            // TODO: Implement YubiKeyStorage
            CASHEW_LOG_WARN("YubiKey support not implemented");
            return nullptr;
            
        case HardwareKeyType::GENERIC_PKCS11:
            // TODO: Implement PKCS11KeyStorage
            CASHEW_LOG_WARN("PKCS#11 support not implemented");
            return nullptr;
            
        default:
            return nullptr;
    }
}

std::vector<HardwareKeyType> HardwareKeyStorageFactory::detect_available_hardware() {
    std::vector<HardwareKeyType> available;
    
    // Always have software fallback
    available.push_back(HardwareKeyType::NONE);
    
    // Check for TPM
    #ifdef CASHEW_PLATFORM_LINUX
    // Check for /dev/tpm0 or /dev/tpmrm0
    // Stub: not implemented
    #endif
    
    #ifdef CASHEW_PLATFORM_WINDOWS
    // Check TBS API
    // Stub: not implemented
    #endif
    
    #ifdef CASHEW_PLATFORM_MACOS
    // Check for Secure Enclave (only on Apple Silicon and T2 Macs)
    // Stub: not implemented
    #endif
    
    return available;
}

// Helper functions

const char* hardware_key_type_to_string(HardwareKeyType type) {
    switch (type) {
        case HardwareKeyType::NONE: return "Software";
        case HardwareKeyType::TPM_2_0: return "TPM 2.0";
        case HardwareKeyType::SECURE_ENCLAVE: return "Secure Enclave";
        case HardwareKeyType::WINDOWS_NGC: return "Windows NGC";
        case HardwareKeyType::YUBIKEY: return "YubiKey";
        case HardwareKeyType::GENERIC_PKCS11: return "PKCS#11";
        default: return "Unknown";
    }
}

} // namespace cashew::security
