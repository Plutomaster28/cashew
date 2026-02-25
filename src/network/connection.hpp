#pragma once

#include "cashew/common.hpp"
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>

namespace cashew::network {

/**
 * ConnectionState - State of a network connection
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    CONN_ERROR  // Renamed from ERROR to avoid Windows macro conflict
};

/**
 * AddressFamily - IP version support
 */
enum class AddressFamily {
    IPv4,
    IPv6,
    ANY  // Let system decide
};

/**
 * SocketAddress - Network address abstraction
 */
struct SocketAddress {
    std::string host;
    uint16_t port;
    AddressFamily family;
    
    SocketAddress() : port(0), family(AddressFamily::ANY) {}
    SocketAddress(const std::string& h, uint16_t p, AddressFamily f = AddressFamily::ANY)
        : host(h), port(p), family(f) {}
    
    std::string to_string() const;
    static std::optional<SocketAddress> from_string(const std::string& addr);
    
    bool is_ipv4() const;
    bool is_ipv6() const;
};

/**
 * BandwidthLimiter - Rate limiting for connections
 */
class BandwidthLimiter {
public:
    BandwidthLimiter(uint64_t bytes_per_second);
    
    // Check if we can send/receive
    bool can_send(size_t bytes);
    bool can_receive(size_t bytes);
    
    // Record bandwidth usage
    void record_sent(size_t bytes);
    void record_received(size_t bytes);
    
    // Get current usage
    uint64_t get_bytes_sent_per_second() const;
    uint64_t get_bytes_received_per_second() const;
    
    // Update limits
    void set_limit(uint64_t bytes_per_second);
    uint64_t get_limit() const { return bytes_per_second_; }
    
private:
    uint64_t bytes_per_second_;
    
    // Token bucket algorithm
    uint64_t tx_tokens_;
    uint64_t rx_tokens_;
    std::chrono::steady_clock::time_point last_update_;
    
    mutable std::mutex mutex_;
    
    void refill_tokens();
};

/**
 * Connection - Abstract base class for network connections
 * 
 * Provides async I/O abstraction that can be implemented with:
 * - Native sockets (current)
 * - Boost.Asio (future)
 * - libuv (alternative)
 */
class Connection {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void()>;
    
    virtual ~Connection() = default;
    
    // Connection lifecycle
    virtual bool connect(const SocketAddress& addr) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Send/receive
    virtual bool send(const std::vector<uint8_t>& data) = 0;
    virtual std::optional<std::vector<uint8_t>> receive(size_t max_bytes) = 0;
    
    // Async operations
    virtual void async_send(const std::vector<uint8_t>& data, std::function<void(bool)> callback) = 0;
    virtual void async_receive(size_t max_bytes, DataCallback callback) = 0;
    
    // State
    virtual ConnectionState get_state() const = 0;
    virtual SocketAddress get_local_address() const = 0;
    virtual SocketAddress get_remote_address() const = 0;
    
    // Callbacks
    void set_data_callback(DataCallback callback) { data_callback_ = callback; }
    void set_error_callback(ErrorCallback callback) { error_callback_ = callback; }
    void set_connected_callback(ConnectedCallback callback) { connected_callback_ = callback; }
    void set_disconnected_callback(DisconnectedCallback callback) { disconnected_callback_ = callback; }
    
    // Bandwidth limiting
    virtual void set_bandwidth_limiter(std::shared_ptr<BandwidthLimiter> limiter) = 0;
    
    // Statistics
    virtual uint64_t bytes_sent() const = 0;
    virtual uint64_t bytes_received() const = 0;
    virtual std::chrono::seconds connection_duration() const = 0;
    
protected:
    DataCallback data_callback_;
    ErrorCallback error_callback_;
    ConnectedCallback connected_callback_;
    DisconnectedCallback disconnected_callback_;
    
    void on_data(const std::vector<uint8_t>& data) {
        if (data_callback_) data_callback_(data);
    }
    
    void on_error(const std::string& error) {
        if (error_callback_) error_callback_(error);
    }
    
    void on_connected() {
        if (connected_callback_) connected_callback_();
    }
    
    void on_disconnected() {
        if (disconnected_callback_) disconnected_callback_();
    }
};

/**
 * TCPConnection - TCP socket connection
 */
class TCPConnection : public Connection {
public:
    TCPConnection();
    ~TCPConnection() override;
    
    bool connect(const SocketAddress& addr) override;
    void disconnect() override;
    bool is_connected() const override;
    
    bool send(const std::vector<uint8_t>& data) override;
    std::optional<std::vector<uint8_t>> receive(size_t max_bytes) override;
    
    void async_send(const std::vector<uint8_t>& data, std::function<void(bool)> callback) override;
    void async_receive(size_t max_bytes, DataCallback callback) override;
    
    ConnectionState get_state() const override { return state_; }
    SocketAddress get_local_address() const override;
    SocketAddress get_remote_address() const override;
    
    void set_bandwidth_limiter(std::shared_ptr<BandwidthLimiter> limiter) override;
    
    uint64_t bytes_sent() const override { return bytes_sent_; }
    uint64_t bytes_received() const override { return bytes_received_; }
    std::chrono::seconds connection_duration() const override;
    
    // TCP-specific options
    void set_nodelay(bool enable);
    void set_keepalive(bool enable, uint32_t idle_seconds = 60);
    
private:
    int socket_fd_;
    ConnectionState state_;
    SocketAddress local_addr_;
    SocketAddress remote_addr_;
    
    std::atomic<uint64_t> bytes_sent_;
    std::atomic<uint64_t> bytes_received_;
    std::chrono::steady_clock::time_point connected_at_;
    
    std::shared_ptr<BandwidthLimiter> bandwidth_limiter_;
    
    // Async I/O thread
    std::thread async_thread_;
    std::atomic<bool> async_running_;
    mutable std::mutex send_mutex_;
    mutable std::mutex receive_mutex_;
    
    bool create_socket(AddressFamily family);
    void close_socket();
    bool set_nonblocking(bool nonblocking);
};

/**
 * ConnectionManager - Manages multiple connections
 */
class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();
    
    // Connection management
    std::shared_ptr<Connection> create_connection(const SocketAddress& addr);
    void close_connection(const std::string& connection_id);
    void close_all_connections();
    
    // Connection lookup
    std::shared_ptr<Connection> get_connection(const std::string& connection_id);
    bool has_connection(const std::string& connection_id) const;
    
    // Statistics
    size_t active_connection_count() const;
    std::vector<std::string> get_active_connections() const;
    
    // Global bandwidth limiting
    void set_global_bandwidth_limit(uint64_t bytes_per_second);
    uint64_t get_global_bandwidth_limit() const;
    
    // Maintenance
    void cleanup_dead_connections();
    
private:
    std::map<std::string, std::shared_ptr<Connection>> connections_;
    std::shared_ptr<BandwidthLimiter> global_limiter_;
    mutable std::mutex mutex_;
    
    std::string generate_connection_id(const SocketAddress& addr);
};

} // namespace cashew::network
