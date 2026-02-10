#include "network/nat_traversal.hpp"
#include <spdlog/spdlog.h>
#include <random>
#include <cstring>

#ifdef CASHEW_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace cashew::network {

// Helper to generate random transaction ID
static std::array<uint8_t, 12> generate_transaction_id() {
    std::array<uint8_t, 12> id;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& byte : id) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    return id;
}

// Helper to write uint16_t in network byte order
static void write_uint16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(static_cast<uint8_t>(value >> 8));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Helper to write uint32_t in network byte order
static void write_uint32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>(value >> 24));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Helper to read uint16_t from network byte order
static uint16_t read_uint16(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) |
           static_cast<uint16_t>(data[1]);
}

// Helper to read uint32_t from network byte order
static uint32_t read_uint32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

// PublicAddress methods
std::string PublicAddress::to_string() const {
    return ip + ":" + std::to_string(port);
}

// STUNMessage methods
std::vector<uint8_t> STUNMessage::to_bytes() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(20 + attributes.size());
    
    // Message type (2 bytes)
    write_uint16(buffer, message_type);
    
    // Message length (2 bytes) - excludes the 20-byte STUN header
    write_uint16(buffer, static_cast<uint16_t>(attributes.size()));
    
    // Magic cookie (4 bytes)
    write_uint32(buffer, magic_cookie);
    
    // Transaction ID (12 bytes)
    buffer.insert(buffer.end(), transaction_id.begin(), transaction_id.end());
    
    // Attributes
    buffer.insert(buffer.end(), attributes.begin(), attributes.end());
    
    return buffer;
}

std::optional<STUNMessage> STUNMessage::from_bytes(const std::vector<uint8_t>& data) {
    if (data.size() < 20) {
        return std::nullopt;  // Minimum STUN message size
    }
    
    STUNMessage msg;
    
    // Parse header
    msg.message_type = read_uint16(data.data());
    msg.message_length = read_uint16(data.data() + 2);
    msg.magic_cookie = read_uint32(data.data() + 4);
    
    // Verify magic cookie
    if (msg.magic_cookie != MAGIC_COOKIE) {
        return std::nullopt;
    }
    
    // Parse transaction ID
    std::copy(data.begin() + 8, data.begin() + 20, msg.transaction_id.begin());
    
    // Parse attributes
    if (data.size() > 20) {
        msg.attributes.assign(data.begin() + 20, data.end());
    }
    
    return msg;
}

std::optional<SocketAddress> STUNMessage::get_mapped_address() const {
    // Parse attributes to find XOR-MAPPED-ADDRESS or MAPPED-ADDRESS
    size_t offset = 0;
    
    while (offset + 4 <= attributes.size()) {
        uint16_t attr_type = read_uint16(attributes.data() + offset);
        uint16_t attr_length = read_uint16(attributes.data() + offset + 2);
        
        if (offset + 4 + attr_length > attributes.size()) {
            break;  // Malformed attribute
        }
        
        if (attr_type == ATTR_XOR_MAPPED_ADDRESS || attr_type == ATTR_MAPPED_ADDRESS) {
            const uint8_t* attr_data = attributes.data() + offset + 4;
            
            if (attr_length < 8) {
                break;  // Too short for address
            }
            
            uint8_t family = attr_data[1];
            uint16_t port = read_uint16(attr_data + 2);
            
            // XOR obfuscation for XOR-MAPPED-ADDRESS
            if (attr_type == ATTR_XOR_MAPPED_ADDRESS) {
                port ^= (magic_cookie >> 16);
            }
            
            if (family == 0x01) {  // IPv4
                uint32_t addr = read_uint32(attr_data + 4);
                
                if (attr_type == ATTR_XOR_MAPPED_ADDRESS) {
                    addr ^= magic_cookie;
                }
                
                // Convert to string
                char ip_str[INET_ADDRSTRLEN];
                struct in_addr in_addr;
                in_addr.s_addr = htonl(addr);
                inet_ntop(AF_INET, &in_addr, ip_str, INET_ADDRSTRLEN);
                
                return SocketAddress(std::string(ip_str), port);
            } else if (family == 0x02) {  // IPv6
                std::array<uint8_t, 16> addr_bytes;
                std::copy(attr_data + 4, attr_data + 20, addr_bytes.begin());
                
                if (attr_type == ATTR_XOR_MAPPED_ADDRESS) {
                    // XOR with magic cookie and transaction ID
                    uint32_t xor_value = magic_cookie;
                    for (size_t i = 0; i < 4; ++i) {
                        addr_bytes[i] ^= (xor_value >> (24 - i * 8)) & 0xFF;
                    }
                    for (size_t i = 0; i < 12; ++i) {
                        addr_bytes[i + 4] ^= transaction_id[i];
                    }
                }
                
                // Convert to string
                char ip_str[INET6_ADDRSTRLEN];
                struct in6_addr in6_addr;
                std::copy(addr_bytes.begin(), addr_bytes.end(), in6_addr.s6_addr);
                inet_ntop(AF_INET6, &in6_addr, ip_str, INET6_ADDRSTRLEN);
                
                return SocketAddress(std::string(ip_str), port);
            }
        }
        
        // Move to next attribute (padding to 4-byte boundary)
        offset += 4 + attr_length;
        if (attr_length % 4 != 0) {
            offset += 4 - (attr_length % 4);
        }
    }
    
    return std::nullopt;
}

// NATTraversal methods
NATTraversal::NATTraversal() 
    : timeout_(std::chrono::milliseconds(3000)) {
    // Initialize with default STUN servers
    stun_servers_ = DEFAULT_STUN_SERVERS;
}

void NATTraversal::add_stun_server(const STUNServer& server) {
    stun_servers_.push_back(server);
}

void NATTraversal::clear_stun_servers() {
    stun_servers_.clear();
}

bool NATTraversal::is_cache_valid() const {
    if (!cached_address_.has_value()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now - cache_time_).count()
    );
    
    return elapsed < CACHE_VALIDITY_SECONDS;
}

std::optional<PublicAddress> NATTraversal::get_cached_address() const {
    if (is_cache_valid()) {
        return cached_address_;
    }
    return std::nullopt;
}

void NATTraversal::clear_cache() {
    cached_address_.reset();
}

STUNMessage NATTraversal::create_binding_request() {
    STUNMessage msg;
    msg.message_type = STUNMessage::BINDING_REQUEST;
    msg.message_length = 0;
    msg.magic_cookie = STUNMessage::MAGIC_COOKIE;
    msg.transaction_id = generate_transaction_id();
    
    return msg;
}

std::optional<PublicAddress> NATTraversal::query_stun_server(const STUNServer& server) {
    spdlog::debug("Querying STUN server: {}:{}", server.host, server.port);
    
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        spdlog::error("Failed to create UDP socket for STUN query");
        return std::nullopt;
    }
    
    // Set socket timeout
#ifdef CASHEW_PLATFORM_WINDOWS
    DWORD timeout_ms = static_cast<DWORD>(timeout_.count());
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
    struct timeval tv;
    tv.tv_sec = timeout_.count() / 1000;
    tv.tv_usec = (timeout_.count() % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    
    // Resolve STUN server address
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    int err = getaddrinfo(server.host.c_str(), std::to_string(server.port).c_str(), &hints, &result);
    if (err != 0 || !result) {
        spdlog::error("Failed to resolve STUN server: {}", server.host);
#ifdef CASHEW_PLATFORM_WINDOWS
        closesocket(sock);
#else
        close(sock);
#endif
        return std::nullopt;
    }
    
    // Create and send STUN binding request
    STUNMessage request = create_binding_request();
    std::vector<uint8_t> request_data = request.to_bytes();
    
    ssize_t sent = sendto(sock, reinterpret_cast<const char*>(request_data.data()), 
                          request_data.size(), 0, result->ai_addr, result->ai_addrlen);
    
    if (sent < 0) {
        spdlog::error("Failed to send STUN binding request");
        freeaddrinfo(result);
#ifdef CASHEW_PLATFORM_WINDOWS
        closesocket(sock);
#else
        close(sock);
#endif
        return std::nullopt;
    }
    
    // Receive response
    std::vector<uint8_t> response_data(1024);
    struct sockaddr_storage from_addr{};
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t received = recvfrom(sock, reinterpret_cast<char*>(response_data.data()),
                                response_data.size(), 0,
                                reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);
    
    freeaddrinfo(result);
#ifdef CASHEW_PLATFORM_WINDOWS
    closesocket(sock);
#else
    close(sock);
#endif
    
    if (received < 0) {
        spdlog::error("Failed to receive STUN response (timeout or error)");
        return std::nullopt;
    }
    
    response_data.resize(received);
    
    // Parse STUN response
    auto response_msg = STUNMessage::from_bytes(response_data);
    if (!response_msg.has_value()) {
        spdlog::error("Failed to parse STUN response");
        return std::nullopt;
    }
    
    // Verify transaction ID matches
    if (response_msg->transaction_id != request.transaction_id) {
        spdlog::error("STUN response transaction ID mismatch");
        return std::nullopt;
    }
    
    // Check message type
    if (response_msg->message_type != STUNMessage::BINDING_RESPONSE) {
        spdlog::error("STUN response is not a binding response");
        return std::nullopt;
    }
    
    // Extract mapped address
    auto mapped_addr = response_msg->get_mapped_address();
    if (!mapped_addr.has_value()) {
        spdlog::error("STUN response missing mapped address");
        return std::nullopt;
    }
    
    PublicAddress public_addr;
    public_addr.ip = mapped_addr->host;
    public_addr.port = mapped_addr->port;
    public_addr.nat_type = NATType::UNKNOWN;  // Will be determined by detect_nat_type
    public_addr.discovered_at = std::chrono::system_clock::now().time_since_epoch().count();
    
    spdlog::info("Discovered public address via STUN: {}", public_addr.to_string());
    
    return public_addr;
}

std::optional<PublicAddress> NATTraversal::discover_public_address() {
    // Check cache first
    if (is_cache_valid()) {
        spdlog::debug("Using cached public address");
        return cached_address_;
    }
    
    // Try each STUN server until one succeeds
    for (const auto& server : stun_servers_) {
        auto result = query_stun_server(server);
        if (result.has_value()) {
            cached_address_ = result;
            cache_time_ = std::chrono::steady_clock::now();
            return result;
        }
    }
    
    spdlog::error("Failed to discover public address from any STUN server");
    return std::nullopt;
}

NATType NATTraversal::detect_nat_type() {
    // Simplified NAT type detection
    // Full RFC 5780 implementation would require multiple STUN queries
    // with different source/destination combinations
    
    auto public_addr = discover_public_address();
    if (!public_addr.has_value()) {
        return NATType::UNKNOWN;
    }
    
    // For now, assume FULL_CONE if we can discover address
    // A complete implementation would:
    // 1. Test if we can receive from different IP (FULL_CONE vs RESTRICTED_CONE)
    // 2. Test if we can receive from different port (RESTRICTED_CONE vs PORT_RESTRICTED)
    // 3. Test if port changes per destination (SYMMETRIC)
    
    spdlog::info("NAT type detection: assuming FULL_CONE (simplified implementation)");
    return NATType::FULL_CONE;
}

} // namespace cashew::network
