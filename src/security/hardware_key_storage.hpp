#pragma once

#include "cashew/common.hpp"
#include <string>
#include <optional>
#include <memory>
#include <map>

namespace cashew::security {

/**
 * HardwareKeyType - Type of hardware security module
 */
enum class HardwareKeyType {
    NONE,           // Software-only storage
    TPM_2_0,        // Trusted Platform Module 2.0
    SECURE_ENCLAVE, // Apple Secure Enclave (macOS/iOS)
    WINDOWS_NGC,    // Windows Hello / Next Generation Credentials
    YUBIKEY,        // YubiKey hardware token
    GENERIC_PKCS11  // Generic PKCS#11 token
};

/**
 * HardwareKeyCapabilities - What the hardware module supports
 */
struct HardwareKeyCapabilities {
    bool can_generate_keys;
    bool can_sign;
    bool can_encrypt;
    bool can_export_public_key;
    bool can_export_private_key;  // Usually false for hardware security
    bool requires_pin;
    bool supports_key_attestation;
    uint32_t max_key_size_bits;
    
    HardwareKeyCapabilities()
        : can_generate_keys(false)
        , can_sign(false)
        , can_encrypt(false)
        , can_export_public_key(false)
        , can_export_private_key(false)
        , requires_pin(false)
        , supports_key_attestation(false)
        , max_key_size_bits(0)
    {}
};

/**
 * HardwareKeyHandle - Opaque handle to hardware-stored key
 */
struct HardwareKeyHandle {
    HardwareKeyType type;
    std::string device_identifier;  // Device serial, path, or ID
    std::string key_identifier;     // Key ID within the device
    bytes attestation_certificate;  // Optional attestation cert
    
    HardwareKeyHandle()
        : type(HardwareKeyType::NONE)
    {}
    
    bool is_valid() const { return type != HardwareKeyType::NONE && !key_identifier.empty(); }
};

/**
 * HardwareKeyStorage - Interface for hardware-backed key storage
 * 
 * Design principles:
 * 1. Optional - gracefully degrades to software storage
 * 2. Platform-specific - implementations for TPM, Secure Enclave, etc.
 * 3. Secure - private keys never leave hardware
 * 4. Attestation - can prove key is hardware-backed
 * 
 * Note: This is an optional security enhancement. The system works
 * fine with software key storage. Hardware-backed keys provide:
 * - Protection against key theft
 * - Key attestation (proof of hardware storage)
 * - Tamper resistance
 * - Regulatory compliance (FIPS 140-2, Common Criteria)
 */
class HardwareKeyStorage {
public:
    virtual ~HardwareKeyStorage() = default;
    
    /**
     * Check if hardware key storage is available on this system
     */
    virtual bool is_available() const = 0;
    
    /**
     * Get capabilities of this hardware module
     */
    virtual HardwareKeyCapabilities get_capabilities() const = 0;
    
    /**
     * Initialize hardware module (may require PIN/password)
     */
    virtual bool initialize(const std::string& pin = "") = 0;
    
    /**
     * Generate a new Ed25519 keypair in hardware
     * @param key_label User-friendly name for the key
     * @return Handle to the hardware key
     */
    virtual std::optional<HardwareKeyHandle> generate_key(const std::string& key_label) = 0;
    
    /**
     * Import an existing private key into hardware (if supported)
     * @param secret_key The key to import
     * @param key_label User-friendly name
     * @return Handle to the hardware key
     */
    virtual std::optional<HardwareKeyHandle> import_key(
        const SecretKey& secret_key,
        const std::string& key_label
    ) = 0;
    
    /**
     * Get public key from hardware
     * @param handle Handle to the key
     * @return The public key
     */
    virtual std::optional<PublicKey> get_public_key(const HardwareKeyHandle& handle) = 0;
    
    /**
     * Sign data using hardware key
     * @param handle Handle to the signing key
     * @param data Data to sign
     * @return Ed25519 signature
     */
    virtual std::optional<Signature> sign(const HardwareKeyHandle& handle, const bytes& data) = 0;
    
    /**
     * Delete key from hardware
     * @param handle Handle to the key
     * @return true if successful
     */
    virtual bool delete_key(const HardwareKeyHandle& handle) = 0;
    
    /**
     * Get attestation certificate for a key (proves it's hardware-backed)
     * @param handle Handle to the key
     * @return Attestation certificate (format depends on hardware type)
     */
    virtual std::optional<bytes> get_attestation(const HardwareKeyHandle& handle) = 0;
    
    /**
     * List all keys stored in hardware
     * @return Vector of key handles
     */
    virtual std::vector<HardwareKeyHandle> list_keys() = 0;
    
    /**
     * Verify attestation certificate
     * @param attestation The attestation to verify
     * @return true if attestation is valid
     */
    virtual bool verify_attestation(const bytes& attestation) = 0;
};

/**
 * SoftwareKeyStorage - Software-only implementation (no hardware)
 * 
 * This is the fallback when no hardware security module is available.
 * Keys are stored in memory and can be persisted to encrypted files.
 */
class SoftwareKeyStorage : public HardwareKeyStorage {
public:
    SoftwareKeyStorage();
    ~SoftwareKeyStorage() override = default;
    
    bool is_available() const override { return true; }
    HardwareKeyCapabilities get_capabilities() const override;
    bool initialize(const std::string& pin = "") override;
    
    std::optional<HardwareKeyHandle> generate_key(const std::string& key_label) override;
    std::optional<HardwareKeyHandle> import_key(const SecretKey& secret_key, const std::string& key_label) override;
    std::optional<PublicKey> get_public_key(const HardwareKeyHandle& handle) override;
    std::optional<Signature> sign(const HardwareKeyHandle& handle, const bytes& data) override;
    bool delete_key(const HardwareKeyHandle& handle) override;
    std::optional<bytes> get_attestation(const HardwareKeyHandle& handle) override;
    std::vector<HardwareKeyHandle> list_keys() override;
    bool verify_attestation(const bytes& attestation) override;
    
private:
    struct KeyEntry {
        std::string label;
        PublicKey public_key;
        SecretKey secret_key;
    };
    
    std::map<std::string, KeyEntry> keys_;
    bool initialized_;
};

/**
 * TPMKeyStorage - TPM 2.0 hardware storage (Linux/Windows)
 * 
 * NOTE: This is a stub implementation. Full TPM support requires:
 * - tpm2-tss library
 * - TPM 2.0 device
 * - Platform-specific initialization
 * 
 * For production use, integrate with:
 * - Linux: tpm2-tss, tpm2-tools
 * - Windows: TBS (TPM Base Services)
 */
class TPMKeyStorage : public HardwareKeyStorage {
public:
    TPMKeyStorage();
    ~TPMKeyStorage() override = default;
    
    bool is_available() const override;
    HardwareKeyCapabilities get_capabilities() const override;
    bool initialize(const std::string& pin = "") override;
    
    std::optional<HardwareKeyHandle> generate_key(const std::string& key_label) override;
    std::optional<HardwareKeyHandle> import_key(const SecretKey& secret_key, const std::string& key_label) override;
    std::optional<PublicKey> get_public_key(const HardwareKeyHandle& handle) override;
    std::optional<Signature> sign(const HardwareKeyHandle& handle, const bytes& data) override;
    bool delete_key(const HardwareKeyHandle& handle) override;
    std::optional<bytes> get_attestation(const HardwareKeyHandle& handle) override;
    std::vector<HardwareKeyHandle> list_keys() override;
    bool verify_attestation(const bytes& attestation) override;
    
private:
    bool tpm_available_;
    bool initialized_;
    // In production: TPM context, session handles, etc.
};

/**
 * HardwareKeyStorageFactory - Create appropriate hardware storage for platform
 */
class HardwareKeyStorageFactory {
public:
    /**
     * Detect and create the best available hardware key storage
     * @return Hardware storage instance (falls back to software)
     */
    static std::unique_ptr<HardwareKeyStorage> create_best_available();
    
    /**
     * Create specific hardware storage type
     * @param type The hardware type to create
     * @return Hardware storage instance, or nullptr if not available
     */
    static std::unique_ptr<HardwareKeyStorage> create(HardwareKeyType type);
    
    /**
     * Detect available hardware security modules on this system
     * @return Vector of available types
     */
    static std::vector<HardwareKeyType> detect_available_hardware();
};

/**
 * Helper to convert HardwareKeyType to string
 */
const char* hardware_key_type_to_string(HardwareKeyType type);

} // namespace cashew::security
