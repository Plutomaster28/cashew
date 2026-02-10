#include "network/connection.hpp"
#include "utils/logger.hpp"
#include <cstring>
#include <sstream>

// Platform-specific includes
#ifdef CASHEW_PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

namespace cashew::network {

// SocketAddress implementation

std::string SocketAddress::to_string() const {
    std::ostringstream oss;
    if (family == AddressFamily::IPv6) {
        oss << "[" << host << "]:" << port;
    } else {
        oss << host << ":" << port;
    }
    return oss.str();
}

std::optional<SocketAddress> SocketAddress::from_string(const std::string& addr) {
    // Parse IPv6: [host]:port
    if (addr[0] == '[') {
        size_t close_bracket = addr.find(']');
        if (close_bracket == std::string::npos) {
            return std::nullopt;
        }
        
        std::string host = addr.substr(1, close_bracket - 1);
        
        if (close_bracket + 1 >= addr.length() || addr[close_bracket + 1] != ':') {
            return std::nullopt;
        }
        
        uint16_t port = static_cast<uint16_t>(std::stoi(addr.substr(close_bracket + 2)));
        return SocketAddress(host, port, AddressFamily::IPv6);
    }
    
    // Parse IPv4: host:port
    size_t colon = addr.rfind(':');
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    
    std::string host = addr.substr(0, colon);
    uint16_t port = static_cast<uint16_t>(std::stoi(addr.substr(colon + 1)));
    
    // Detect if it's IPv6 without brackets
    if (host.find(':') != std::string::npos) {
        return SocketAddress(host, port, AddressFamily::IPv6);
    }
    
    return SocketAddress(host, port, AddressFamily::IPv4);
}

bool SocketAddress::is_ipv4() const {
    return family == AddressFamily::IPv4 || 
           (family == AddressFamily::ANY && host.find(':') == std::string::npos);
}

bool SocketAddress::is_ipv6() const {
    return family == AddressFamily::IPv6 || 
           (family == AddressFamily::ANY && host.find(':') != std::string::npos);
}

// BandwidthLimiter implementation

BandwidthLimiter::BandwidthLimiter(uint64_t bytes_per_second)
    : bytes_per_second_(bytes_per_second),
      tx_tokens_(bytes_per_second),
      rx_tokens_(bytes_per_second),
      last_update_(std::chrono::steady_clock::now()) {
}

void BandwidthLimiter::refill_tokens() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_);
    
    if (elapsed.count() > 0) {
        uint64_t tokens_to_add = (bytes_per_second_ * elapsed.count()) / 1000;
        
        tx_tokens_ = std::min(tx_tokens_ + tokens_to_add, bytes_per_second_);
        rx_tokens_ = std::min(rx_tokens_ + tokens_to_add, bytes_per_second_);
        
        last_update_ = now;
    }
}

bool BandwidthLimiter::can_send(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_tokens();
    return tx_tokens_ >= bytes;
}

bool BandwidthLimiter::can_receive(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_tokens();
    return rx_tokens_ >= bytes;
}

void BandwidthLimiter::record_sent(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_tokens();
    if (tx_tokens_ >= bytes) {
        tx_tokens_ -= bytes;
    } else {
        tx_tokens_ = 0;
    }
}

void BandwidthLimiter::record_received(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_tokens();
    if (rx_tokens_ >= bytes) {
        rx_tokens_ -= bytes;
    } else {
        rx_tokens_ = 0;
    }
}

uint64_t BandwidthLimiter::get_bytes_sent_per_second() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bytes_per_second_ - tx_tokens_;
}

uint64_t BandwidthLimiter::get_bytes_received_per_second() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bytes_per_second_ - rx_tokens_;
}

void BandwidthLimiter::set_limit(uint64_t bytes_per_second) {
    std::lock_guard<std::mutex> lock(mutex_);
    bytes_per_second_ = bytes_per_second;
}

// TCPConnection implementation

#ifdef CASHEW_PLATFORM_WINDOWS
struct WindowsSocketInitializer {
    WindowsSocketInitializer() {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
    }
    ~WindowsSocketInitializer() {
        WSACleanup();
    }
};

static WindowsSocketInitializer g_winsock_init;
#endif

TCPConnection::TCPConnection()
    : socket_fd_(INVALID_SOCKET),
      state_(ConnectionState::DISCONNECTED),
      bytes_sent_(0),
      bytes_received_(0),
      async_running_(false) {
}

TCPConnection::~TCPConnection() {
    disconnect();
}

bool TCPConnection::create_socket(AddressFamily family) {
    int af = (family == AddressFamily::IPv6) ? AF_INET6 : AF_INET;
    
    socket_fd_ = socket(af, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd_ == INVALID_SOCKET) {
        CASHEW_LOG_ERROR("Failed to create socket: {}", strerror(errno));
        return false;
    }
    
    // Set socket options
    int reuse = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    
    return true;
}

void TCPConnection::close_socket() {
    if (socket_fd_ != INVALID_SOCKET) {
#ifdef CASHEW_PLATFORM_WINDOWS
        closesocket(socket_fd_);
#else
        ::close(socket_fd_);
#endif
        socket_fd_ = INVALID_SOCKET;
    }
}

bool TCPConnection::set_nonblocking(bool nonblocking) {
#ifdef CASHEW_PLATFORM_WINDOWS
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket(socket_fd_, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags == -1) return false;
    
    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    return fcntl(socket_fd_, F_SETFL, flags) == 0;
#endif
}

bool TCPConnection::connect(const SocketAddress& addr) {
    if (state_ != ConnectionState::DISCONNECTED) {
        CASHEW_LOG_WARN("Connection already active");
        return false;
    }
    
    state_ = ConnectionState::CONNECTING;
    remote_addr_ = addr;
    
    // Create socket
    AddressFamily family = addr.is_ipv6() ? AddressFamily::IPv6 : AddressFamily::IPv4;
    if (!create_socket(family)) {
        state_ = ConnectionState::ERROR;
        return false;
    }
    
    // Resolve address
    struct addrinfo hints, *result = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = (family == AddressFamily::IPv6) ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    std::string port_str = std::to_string(addr.port);
    int ret = getaddrinfo(addr.host.c_str(), port_str.c_str(), &hints, &result);
    if (ret != 0) {
        CASHEW_LOG_ERROR("Failed to resolve address {}: {}", addr.to_string(), gai_strerror(ret));
        close_socket();
        state_ = ConnectionState::ERROR;
        return false;
    }
    
    // Connect
    bool connected = false;
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        ret = ::connect(socket_fd_, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            connected = true;
            break;
        }
    }
    
    freeaddrinfo(result);
    
    if (!connected) {
        CASHEW_LOG_ERROR("Failed to connect to {}: {}", addr.to_string(), strerror(errno));
        close_socket();
        state_ = ConnectionState::ERROR;
        return false;
    }
    
    state_ = ConnectionState::CONNECTED;
    connected_at_ = std::chrono::steady_clock::now();
    
    // Get local address
    struct sockaddr_storage local_storage;
    socklen_t addr_len = sizeof(local_storage);
    if (getsockname(socket_fd_, reinterpret_cast<struct sockaddr*>(&local_storage), &addr_len) == 0) {
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];
        getnameinfo(reinterpret_cast<struct sockaddr*>(&local_storage), addr_len,
                   host, sizeof(host), service, sizeof(service),
                   NI_NUMERICHOST | NI_NUMERICSERV);
        
        uint16_t local_port = static_cast<uint16_t>(std::stoi(service));
        local_addr_ = SocketAddress(host, local_port, family);
    }
    
    CASHEW_LOG_INFO("Connected to {} from {}", remote_addr_.to_string(), local_addr_.to_string());
    on_connected();
    
    return true;
}

void TCPConnection::disconnect() {
    if (state_ == ConnectionState::DISCONNECTED) {
        return;
    }
    
    state_ = ConnectionState::DISCONNECTING;
    
    // Stop async operations
    async_running_ = false;
    if (async_thread_.joinable()) {
        async_thread_.join();
    }
    
    close_socket();
    state_ = ConnectionState::DISCONNECTED;
    
    CASHEW_LOG_DEBUG("Disconnected from {}", remote_addr_.to_string());
    on_disconnected();
}

bool TCPConnection::is_connected() const {
    return state_ == ConnectionState::CONNECTED;
}

bool TCPConnection::send(const std::vector<uint8_t>& data) {
    if (!is_connected()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    // Check bandwidth limit
    if (bandwidth_limiter_ && !bandwidth_limiter_->can_send(data.size())) {
        CASHEW_LOG_DEBUG("Send blocked by bandwidth limiter");
        return false;
    }
    
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = ::send(socket_fd_, 
                             reinterpret_cast<const char*>(data.data() + total_sent),
                             data.size() - total_sent, 0);
        
        if (sent == SOCKET_ERROR) {
#ifdef CASHEW_PLATFORM_WINDOWS
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
                // Would block - try again
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            CASHEW_LOG_ERROR("Send failed: {}", strerror(errno));
            on_error("Send failed");
            return false;
        }
        
        total_sent += sent;
    }
    
    bytes_sent_ += data.size();
    
    if (bandwidth_limiter_) {
        bandwidth_limiter_->record_sent(data.size());
    }
    
    return true;
}

std::optional<std::vector<uint8_t>> TCPConnection::receive(size_t max_bytes) {
    if (!is_connected()) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(receive_mutex_);
    
    // Check bandwidth limit
    if (bandwidth_limiter_ && !bandwidth_limiter_->can_receive(max_bytes)) {
        return std::nullopt;
    }
    
    std::vector<uint8_t> buffer(max_bytes);
    ssize_t received = ::recv(socket_fd_, 
                             reinterpret_cast<char*>(buffer.data()),
                             max_bytes, 0);
    
    if (received == SOCKET_ERROR) {
#ifdef CASHEW_PLATFORM_WINDOWS
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
            return std::nullopt;
        }
        
        CASHEW_LOG_ERROR("Receive failed: {}", strerror(errno));
        on_error("Receive failed");
        return std::nullopt;
    }
    
    if (received == 0) {
        // Connection closed
        CASHEW_LOG_DEBUG("Connection closed by peer");
        return std::nullopt;
    }
    
    buffer.resize(received);
    bytes_received_ += received;
    
    if (bandwidth_limiter_) {
        bandwidth_limiter_->record_received(received);
    }
    
    return buffer;
}

void TCPConnection::async_send(const std::vector<uint8_t>& data, std::function<void(bool)> callback) {
    std::thread([this, data, callback]() {
        bool result = send(data);
        if (callback) {
            callback(result);
        }
    }).detach();
}

void TCPConnection::async_receive(size_t max_bytes, DataCallback callback) {
    std::thread([this, max_bytes, callback]() {
        auto result = receive(max_bytes);
        if (result && callback) {
            callback(*result);
        }
    }).detach();
}

SocketAddress TCPConnection::get_local_address() const {
    return local_addr_;
}

SocketAddress TCPConnection::get_remote_address() const {
    return remote_addr_;
}

void TCPConnection::set_bandwidth_limiter(std::shared_ptr<BandwidthLimiter> limiter) {
    bandwidth_limiter_ = limiter;
}

std::chrono::seconds TCPConnection::connection_duration() const {
    if (state_ != ConnectionState::CONNECTED) {
        return std::chrono::seconds(0);
    }
    
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - connected_at_);
}

void TCPConnection::set_nodelay(bool enable) {
    if (socket_fd_ == INVALID_SOCKET) {
        return;
    }
    
    int flag = enable ? 1 : 0;
    setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, 
               reinterpret_cast<const char*>(&flag), sizeof(flag));
}

void TCPConnection::set_keepalive(bool enable, uint32_t idle_seconds) {
    if (socket_fd_ == INVALID_SOCKET) {
        return;
    }
    
    int flag = enable ? 1 : 0;
    setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, 
               reinterpret_cast<const char*>(&flag), sizeof(flag));
    
#ifndef CASHEW_PLATFORM_WINDOWS
    if (enable) {
        int idle = static_cast<int>(idle_seconds);
        setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    }
#endif
}

// ConnectionManager implementation

ConnectionManager::ConnectionManager()
    : global_limiter_(std::make_shared<BandwidthLimiter>(10 * 1024 * 1024)) {  // 10 MB/s default
}

ConnectionManager::~ConnectionManager() {
    close_all_connections();
}

std::shared_ptr<Connection> ConnectionManager::create_connection(const SocketAddress& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string conn_id = generate_connection_id(addr);
    
    // Check if already exists
    auto it = connections_.find(conn_id);
    if (it != connections_.end()) {
        return it->second;
    }
    
    // Create new connection
    auto connection = std::make_shared<TCPConnection>();
    connection->set_bandwidth_limiter(global_limiter_);
    
    if (!connection->connect(addr)) {
        return nullptr;
    }
    
    connections_[conn_id] = connection;
    CASHEW_LOG_INFO("Created connection to {}", addr.to_string());
    
    return connection;
}

void ConnectionManager::close_connection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        it->second->disconnect();
        connections_.erase(it);
    }
}

void ConnectionManager::close_all_connections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, conn] : connections_) {
        conn->disconnect();
    }
    
    connections_.clear();
    CASHEW_LOG_INFO("Closed all connections");
}

std::shared_ptr<Connection> ConnectionManager::get_connection(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool ConnectionManager::has_connection(const std::string& connection_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.find(connection_id) != connections_.end();
}

size_t ConnectionManager::active_connection_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.size();
}

std::vector<std::string> ConnectionManager::get_active_connections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> result;
    result.reserve(connections_.size());
    
    for (const auto& [id, conn] : connections_) {
        result.push_back(id);
    }
    
    return result;
}

void ConnectionManager::set_global_bandwidth_limit(uint64_t bytes_per_second) {
    std::lock_guard<std::mutex> lock(mutex_);
    global_limiter_->set_limit(bytes_per_second);
}

uint64_t ConnectionManager::get_global_bandwidth_limit() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return global_limiter_->get_limit();
}

void ConnectionManager::cleanup_dead_connections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> to_remove;
    
    for (const auto& [id, conn] : connections_) {
        if (!conn->is_connected()) {
            to_remove.push_back(id);
        }
    }
    
    for (const auto& id : to_remove) {
        connections_.erase(id);
    }
    
    if (!to_remove.empty()) {
        CASHEW_LOG_DEBUG("Cleaned up {} dead connections", to_remove.size());
    }
}

std::string ConnectionManager::generate_connection_id(const SocketAddress& addr) {
    return addr.to_string();
}

} // namespace cashew::network
