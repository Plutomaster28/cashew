#pragma once

#include "cashew/common.hpp"
#include "network/connection.hpp"
#include <string>
#include <memory>
#include <optional>

namespace cashew::network {

/**
 * NATType - Detected NAT configuration
 */
enum class NATType {
    OPEN_INTERNET,      // No NAT, direct public IP
    FULL_CONE,          // Same public port for all destinations
    RESTRICTED_CONE,    // Same public port, filtered by destination IP
    PORT_RESTRICTED,    // Same public port, filtered by destination IP:port
    SYMMETRIC,          // Different public port per destination
    UNKNOWN
};

/**
 * PublicAddress - Discovered public address from STUN
 */
struct PublicAddress {
    std::string ip;
    uint16_t port;
    NATType nat_type;
    uint64_t discovered_at;  // Timestamp
    
    PublicAddress() : port(0), nat_type(NATType::UNKNOWN), discovered_at(0) {}
    
    bool is_valid() const {
        return !ip.empty() && port != 0;
    }
    
    std::string to_string() const;
};

/**
 * STUNServer - STUN server configuration
 */
struct STUNServer {
    std::string host;
    uint16_t port;
    
    STUNServer() : port(3478) {}  // Default STUN port
    STUNServer(const std::string& h, uint16_t p) : host(h), port(p) {}
    
    SocketAddress to_socket_address() const {
        return SocketAddress(host, port);
    }
};

/**
 * STUNMessage - STUN protocol message
 * Simplified STUN implementation (RFC 5389)
 */
struct STUNMessage {
    // Message types
    static constexpr uint16_t BINDING_REQUEST = 0x0001;
    static constexpr uint16_t BINDING_RESPONSE = 0x0101;
    static constexpr uint16_t BINDING_ERROR = 0x0111;
    
    // Attributes
    static constexpr uint16_t ATTR_MAPPED_ADDRESS = 0x0001;
    static constexpr uint16_t ATTR_XOR_MAPPED_ADDRESS = 0x0020;
    static constexpr uint16_t ATTR_CHANGED_ADDRESS = 0x0005;
    
    uint16_t message_type;
    uint16_t message_length;
    uint32_t magic_cookie;
    std::array<uint8_t, 12> transaction_id;
    std::vector<uint8_t> attributes;
    
    static constexpr uint32_t MAGIC_COOKIE = 0x2112A442;
    
    std::vector<uint8_t> to_bytes() const;
    static std::optional<STUNMessage> from_bytes(const std::vector<uint8_t>& data);
    
    // Helper to parse XOR-MAPPED-ADDRESS attribute
    std::optional<SocketAddress> get_mapped_address() const;
};

/**
 * NATTraversal - STUN-like NAT traversal implementation
 */
class NATTraversal {
public:
    NATTraversal();
    ~NATTraversal() = default;
    
    // Configuration
    void add_stun_server(const STUNServer& server);
    void clear_stun_servers();
    std::vector<STUNServer> get_stun_servers() const { return stun_servers_; }
    
    // NAT detection
    std::optional<PublicAddress> discover_public_address();
    NATType detect_nat_type();
    
    // Cache management
    std::optional<PublicAddress> get_cached_address() const;
    void clear_cache();
    
    // Timeout configuration
    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
    std::chrono::milliseconds get_timeout() const { return timeout_; }
    
private:
    std::vector<STUNServer> stun_servers_;
    std::optional<PublicAddress> cached_address_;
    std::chrono::milliseconds timeout_;
    std::chrono::steady_clock::time_point cache_time_;
    
    static constexpr uint64_t CACHE_VALIDITY_SECONDS = 300;  // 5 minutes
    
    bool is_cache_valid() const;
    std::optional<PublicAddress> query_stun_server(const STUNServer& server);
    STUNMessage create_binding_request();
};

// Default STUN servers (public Google STUN servers)
const std::vector<STUNServer> DEFAULT_STUN_SERVERS = {
    STUNServer("stun.l.google.com", 19302),
    STUNServer("stun1.l.google.com", 19302),
    STUNServer("stun2.l.google.com", 19302),
    STUNServer("stun3.l.google.com", 19302),
    STUNServer("stun4.l.google.com", 19302)
};

} // namespace cashew::network
