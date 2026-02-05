#pragma once

#include "cashew/common.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

namespace cashew {
namespace gateway {

/**
 * WebSocket message types
 */
enum class WsMessageType {
    TEXT,
    BINARY,
    PING,
    PONG,
    CLOSE
};

/**
 * WebSocket frame
 */
struct WsFrame {
    WsMessageType type;
    std::vector<uint8_t> payload;
    bool is_final{true};
};

/**
 * WebSocket connection state
 */
enum class WsConnectionState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

/**
 * WebSocket connection
 */
class WsConnection {
public:
    explicit WsConnection(const std::string& conn_id);
    
    const std::string& id() const { return connection_id_; }
    WsConnectionState state() const { return state_; }
    
    /**
     * Send text message
     */
    void send_text(const std::string& message);
    
    /**
     * Send binary message
     */
    void send_binary(const std::vector<uint8_t>& data);
    
    /**
     * Send ping frame
     */
    void send_ping();
    
    /**
     * Close connection
     */
    void close(uint16_t code = 1000, const std::string& reason = "");
    
    /**
     * Check if connection is alive
     */
    bool is_alive() const;
    
    /**
     * Update last activity time
     */
    void update_activity();
    
    /**
     * Get time since last activity
     */
    std::chrono::seconds time_since_activity() const;

private:
    std::string connection_id_;
    WsConnectionState state_;
    std::chrono::system_clock::time_point last_activity_;
    mutable std::mutex mutex_;
};

/**
 * WebSocket event types for subscriptions
 */
enum class WsEventType {
    NETWORK_UPDATE,      // Network state changes
    THING_UPDATE,        // Thing content changes
    LEDGER_EVENT,        // New ledger events
    PEER_STATUS,         // Peer connection status
    REPUTATION_CHANGE,   // Reputation updates
    GOSSIP_MESSAGE       // Gossip protocol messages
};

/**
 * WebSocket message handler function
 */
using WsMessageHandler = std::function<void(
    std::shared_ptr<WsConnection> conn,
    const WsFrame& frame
)>;

/**
 * WebSocket event callback
 */
using WsEventCallback = std::function<void(
    std::shared_ptr<WsConnection> conn,
    const std::string& event_data
)>;

/**
 * WebSocket handler configuration
 */
struct WsHandlerConfig {
    std::chrono::seconds ping_interval{30};
    std::chrono::seconds timeout{300};  // 5 minutes
    size_t max_connections{1000};
    size_t max_message_size{1024 * 1024};  // 1 MB
    size_t max_subscriptions_per_client{50};
};

/**
 * WebSocket handler
 * 
 * Manages WebSocket connections for real-time updates and bidirectional
 * communication between the gateway and browser clients.
 */
class WebSocketHandler {
public:
    /**
     * Construct WebSocket handler
     * @param config Handler configuration
     */
    explicit WebSocketHandler(const WsHandlerConfig& config);
    
    ~WebSocketHandler();
    
    /**
     * Start the handler (background thread for keepalive)
     */
    void start();
    
    /**
     * Stop the handler
     */
    void stop();
    
    /**
     * Handle new WebSocket connection
     * @param conn_id Unique connection identifier
     * @return Connection object
     */
    std::shared_ptr<WsConnection> accept_connection(const std::string& conn_id);
    
    /**
     * Handle incoming WebSocket frame
     * @param conn Connection
     * @param frame Received frame
     */
    void handle_frame(std::shared_ptr<WsConnection> conn, const WsFrame& frame);
    
    /**
     * Broadcast message to all connections
     * @param message Message to broadcast
     */
    void broadcast_text(const std::string& message);
    
    /**
     * Broadcast to connections subscribed to an event type
     * @param event_type Event type
     * @param event_data Event data (JSON)
     */
    void broadcast_event(WsEventType event_type, const std::string& event_data);
    
    /**
     * Subscribe connection to event type
     * @param conn Connection
     * @param event_type Event type to subscribe to
     */
    void subscribe(std::shared_ptr<WsConnection> conn, WsEventType event_type);
    
    /**
     * Unsubscribe connection from event type
     * @param conn Connection
     * @param event_type Event type to unsubscribe from
     */
    void unsubscribe(std::shared_ptr<WsConnection> conn, WsEventType event_type);
    
    /**
     * Register message handler for connection
     * @param handler Handler function
     */
    void set_message_handler(WsMessageHandler handler);
    
    /**
     * Get statistics
     */
    struct Statistics {
        size_t active_connections{0};
        size_t total_messages_sent{0};
        size_t total_messages_received{0};
        size_t total_bytes_sent{0};
        size_t total_bytes_received{0};
        size_t subscription_count{0};
    };
    
    Statistics get_statistics() const;

private:
    /**
     * Keepalive loop (pings + timeout checks)
     */
    void keepalive_loop();
    
    /**
     * Clean up dead connections
     */
    void cleanup_connections();
    
    /**
     * Handle control frame (ping/pong/close)
     */
    void handle_control_frame(
        std::shared_ptr<WsConnection> conn,
        const WsFrame& frame
    );
    
    WsHandlerConfig config_;
    
    std::atomic<bool> running_{false};
    std::thread keepalive_thread_;
    
    // Connection management
    mutable std::mutex connections_mutex_;
    std::unordered_map<std::string, std::shared_ptr<WsConnection>> connections_;
    
    // Event subscriptions
    mutable std::mutex subscriptions_mutex_;
    std::unordered_map<WsEventType, std::vector<std::weak_ptr<WsConnection>>> subscriptions_;
    
    // Message handler
    WsMessageHandler message_handler_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
};

/**
 * Parse WebSocket event type from string
 */
WsEventType parse_event_type(const std::string& type_str);

/**
 * Convert event type to string
 */
std::string event_type_to_string(WsEventType type);

} // namespace gateway
} // namespace cashew
