#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <optional>

// Cashew Framework Version
#define CASHEW_VERSION_MAJOR 0
#define CASHEW_VERSION_MINOR 1
#define CASHEW_VERSION_PATCH 0
#define CASHEW_VERSION_STRING "0.1.0"

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #ifndef CASHEW_PLATFORM_WINDOWS
        #define CASHEW_PLATFORM_WINDOWS
    #endif
#elif defined(__linux__)
    #ifndef CASHEW_PLATFORM_LINUX
        #define CASHEW_PLATFORM_LINUX
    #endif
#elif defined(__APPLE__)
    #ifndef CASHEW_PLATFORM_MACOS
        #define CASHEW_PLATFORM_MACOS
    #endif
#endif

// Compiler detection
#if defined(_MSC_VER)
    #define CASHEW_COMPILER_MSVC
#elif defined(__GNUC__)
    #define CASHEW_COMPILER_GCC
#elif defined(__clang__)
    #define CASHEW_COMPILER_CLANG
#endif

// Export macros for shared libraries
#ifdef CASHEW_PLATFORM_WINDOWS
    #ifdef CASHEW_BUILD_SHARED
        #define CASHEW_API __declspec(dllexport)
    #else
        #define CASHEW_API __declspec(dllimport)
    #endif
#else
    #define CASHEW_API __attribute__((visibility("default")))
#endif

// Debug/Release macros
#ifdef CASHEW_DEBUG
    #define CASHEW_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                std::cerr << "Assertion failed: " << message << std::endl; \
                std::abort(); \
            } \
        } while (0)
#else
    #define CASHEW_ASSERT(condition, message) ((void)0)
#endif

// Utility macros
#define CASHEW_UNUSED(x) (void)(x)
#define CASHEW_DISALLOW_COPY(TypeName) \
    TypeName(const TypeName&) = delete; \
    TypeName& operator=(const TypeName&) = delete

#define CASHEW_DISALLOW_MOVE(TypeName) \
    TypeName(TypeName&&) = delete; \
    TypeName& operator=(TypeName&&) = delete

#define CASHEW_DISALLOW_COPY_AND_MOVE(TypeName) \
    CASHEW_DISALLOW_COPY(TypeName); \
    CASHEW_DISALLOW_MOVE(TypeName)

// Constants
namespace cashew {
namespace constants {

// Network constants
constexpr uint16_t DEFAULT_PORT = 7777;
constexpr size_t MAX_THING_SIZE = 500 * 1024 * 1024; // 500 MB
constexpr size_t MAX_MESSAGE_SIZE = 10 * 1024 * 1024; // 10 MB
constexpr size_t GOSSIP_FANOUT = 3;
constexpr uint32_t EPOCH_DURATION_SECONDS = 600; // 10 minutes
constexpr uint32_t SESSION_TIMEOUT_SECONDS = 1800; // 30 minutes

// Cryptography constants
constexpr size_t ED25519_PUBLIC_KEY_SIZE = 32;
constexpr size_t ED25519_SECRET_KEY_SIZE = 64;  // libsodium uses 64 bytes (32-byte seed + 32-byte public key)
constexpr size_t ED25519_SIGNATURE_SIZE = 64;
constexpr size_t X25519_PUBLIC_KEY_SIZE = 32;
constexpr size_t X25519_SECRET_KEY_SIZE = 32;
constexpr size_t CHACHA20_KEY_SIZE = 32;
constexpr size_t CHACHA20_NONCE_SIZE = 12;
constexpr size_t POLY1305_TAG_SIZE = 16;
constexpr size_t BLAKE3_HASH_SIZE = 32;
constexpr size_t SHA256_HASH_SIZE = 32;

// Node constants
constexpr size_t MIN_NETWORK_QUORUM = 3;
constexpr size_t MAX_NETWORK_QUORUM = 20;
constexpr uint32_t KEY_DECAY_PERIOD_SECONDS = 86400 * 30; // 30 days
constexpr uint8_t MAX_ROUTING_HOPS = 8;

} // namespace constants
} // namespace cashew

// Core types
namespace cashew {

// Basic types
using byte = uint8_t;
using bytes = std::vector<byte>;

// Cryptographic types
template<size_t N>
using fixed_bytes = std::array<byte, N>;

using Hash256 = fixed_bytes<32>;
using PublicKey = fixed_bytes<32>;
using SecretKey = fixed_bytes<64>;  // Ed25519 secret key is 64 bytes in libsodium
using Signature = fixed_bytes<64>;
using Nonce = fixed_bytes<12>;
using SessionKey = fixed_bytes<32>;

// ID types
struct NodeID {
    Hash256 id;
    
    NodeID() = default;
    explicit NodeID(const Hash256& hash) : id(hash) {}
    
    std::string to_string() const;
    static NodeID from_string(const std::string& str);
    
    bool operator==(const NodeID& other) const { return id == other.id; }
    bool operator!=(const NodeID& other) const { return id != other.id; }
    bool operator<(const NodeID& other) const { return id < other.id; }
};

struct HumanID {
    Hash256 id;
    
    HumanID() = default;
    explicit HumanID(const Hash256& hash) : id(hash) {}
    
    std::string to_string() const;
    static HumanID from_string(const std::string& str);
    
    bool operator==(const HumanID& other) const { return id == other.id; }
    bool operator!=(const HumanID& other) const { return id != other.id; }
    bool operator<(const HumanID& other) const { return id < other.id; }
};

struct NetworkID {
    Hash256 id;
    
    NetworkID() = default;
    explicit NetworkID(const Hash256& hash) : id(hash) {}
    
    std::string to_string() const;
    static NetworkID from_string(const std::string& str);
    
    bool operator==(const NetworkID& other) const { return id == other.id; }
    bool operator!=(const NetworkID& other) const { return id != other.id; }
};

struct ContentHash {
    Hash256 hash;
    
    ContentHash() = default;
    explicit ContentHash(const Hash256& h) : hash(h) {}
    
    std::string to_string() const;
    static ContentHash from_string(const std::string& str);
    
    bool operator==(const ContentHash& other) const { return hash == other.hash; }
    bool operator!=(const ContentHash& other) const { return hash != other.hash; }
    bool operator<(const ContentHash& other) const { return hash < other.hash; }
};

// Utility functions
std::string hash_to_hex(const Hash256& hash);
Hash256 hex_to_hash(const std::string& hex);

// Base64 encoding/decoding
std::string base64_encode(const bytes& data);
std::string base64_encode(const void* data, size_t len);
bytes base64_decode(const std::string& encoded);

} // namespace cashew

// Hash support for std::unordered_map
namespace std {
template<>
struct hash<cashew::Hash256> {
    size_t operator()(const cashew::Hash256& h) const noexcept {
        // Hash first 8 bytes
        size_t result = 0;
        for (size_t i = 0; i < 8 && i < h.size(); ++i) {
            result = (result << 8) | h[i];
        }
        return result;
    }
};

template<>
struct hash<cashew::NodeID> {
    size_t operator()(const cashew::NodeID& id) const noexcept {
        return hash<string>()(id.to_string());
    }
};

template<>
struct hash<cashew::ContentHash> {
    size_t operator()(const cashew::ContentHash& h) const noexcept {
        return hash<string>()(h.to_string());
    }
};
} // namespace std
