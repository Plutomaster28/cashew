#include "cashew/gateway/websocket_handler.hpp"
#include "../utils/logger.hpp"
#include <algorithm>

namespace cashew {
namespace gateway {

// ===== WsConnection Implementation =====

WsConnection::WsConnection(const std::string& conn_id)
    : connection_id_(conn_id)
    , state_(WsConnectionState::CONNECTING)
    , last_activity_(std::chrono::system_clock::now())
{}

void WsConnection::send_text(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != WsConnectionState::OPEN) {
        CASHEW_LOG_WARN("Attempted to send on non-open connection: {}", connection_id_);
        return;
    }
    
    // In real implementation, this would encode and send via socket
    CASHEW_LOG_DEBUG("WS [{}] Sending text: {} bytes", connection_id_, message.size());
    update_activity();
}

void WsConnection::send_binary(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != WsConnectionState::OPEN) {
        return;
    }
    
    CASHEW_LOG_DEBUG("WS [{}] Sending binary: {} bytes", connection_id_, data.size());
    update_activity();
}

void WsConnection::send_ping() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != WsConnectionState::OPEN) {
        return;
    }
    
    CASHEW_LOG_TRACE("WS [{}] Sending ping", connection_id_);
    update_activity();
}

void WsConnection::close(uint16_t code, const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == WsConnectionState::CLOSED) {
        return;
    }
    
    CASHEW_LOG_INFO("WS [{}] Closing: code={}, reason={}", 
                    connection_id_, code, reason);
    state_ = WsConnectionState::CLOSING;
    
    // Send close frame
    // In real implementation, would encode close frame and send
    
    state_ = WsConnectionState::CLOSED;
}

bool WsConnection::is_alive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == WsConnectionState::OPEN;
}

void WsConnection::update_activity() {
    last_activity_ = std::chrono::system_clock::now();
}

std::chrono::seconds WsConnection::time_since_activity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - last_activity_);
}

// ===== WebSocketHandler Implementation =====

WebSocketHandler::WebSocketHandler(const WsHandlerConfig& config)
    : config_(config)
{}

WebSocketHandler::~WebSocketHandler() {
    if (running_) {
        stop();
    }
}

void WebSocketHandler::start() {
    if (running_) {
        CASHEW_LOG_WARN("WebSocket handler already running");
        return;
    }
    
    CASHEW_LOG_INFO("Starting WebSocket handler");
    running_ = true;
    
    keepalive_thread_ = std::thread([this]() {
        keepalive_loop();
    });
}

void WebSocketHandler::stop() {
    if (!running_) {
        return;
    }
    
    CASHEW_LOG_INFO("Stopping WebSocket handler");
    running_ = false;
    
    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }
    
    // Close all connections
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [id, conn] : connections_) {
        conn->close(1001, "Server shutdown");
    }
    connections_.clear();
}

std::shared_ptr<WsConnection> WebSocketHandler::accept_connection(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    if (connections_.size() >= config_.max_connections) {
        CASHEW_LOG_WARN("WebSocket connection limit reached");
        return nullptr;
    }
    
    auto conn = std::make_shared<WsConnection>(conn_id);
    connections_[conn_id] = conn;
    
    CASHEW_LOG_INFO("WS connection accepted: {}", conn_id);
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.active_connections++;
    
    return conn;
}

void WebSocketHandler::handle_frame(std::shared_ptr<WsConnection> conn, const WsFrame& frame) {
    if (!conn || !conn->is_alive()) {
        return;
    }
    
    conn->update_activity();
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_messages_received++;
    stats_.total_bytes_received += frame.payload.size();
    
    // Handle control frames
    if (frame.type == WsMessageType::PING ||
        frame.type == WsMessageType::PONG ||
        frame.type == WsMessageType::CLOSE) {
        handle_control_frame(conn, frame);
        return;
    }
    
    // Check message size
    if (frame.payload.size() > config_.max_message_size) {
        CASHEW_LOG_WARN("WS [{}] Message too large: {} bytes", 
                        conn->id(), frame.payload.size());
        conn->close(1009, "Message too large");
        return;
    }
    
    // Call user message handler
    if (message_handler_) {
        try {
            message_handler_(conn, frame);
        } catch (const std::exception& e) {
            CASHEW_LOG_ERROR("WS message handler error: {}", e.what());
        }
    }
}

void WebSocketHandler::broadcast_text(const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    for (auto& [id, conn] : connections_) {
        if (conn->is_alive()) {
            conn->send_text(message);
        }
    }
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_messages_sent += connections_.size();
    stats_.total_bytes_sent += message.size() * connections_.size();
}

void WebSocketHandler::broadcast_event(WsEventType event_type, const std::string& event_data) {
    std::lock_guard<std::mutex> sub_lock(subscriptions_mutex_);
    
    auto it = subscriptions_.find(event_type);
    if (it == subscriptions_.end()) {
        return;
    }
    
    // Build event message
    std::string message = "{\"type\":\"" + event_type_to_string(event_type) + 
                         "\",\"data\":" + event_data + "}";
    
    size_t sent_count = 0;
    
    // Send to all subscribers
    auto& subscribers = it->second;
    for (auto weak_it = subscribers.begin(); weak_it != subscribers.end();) {
        if (auto conn = weak_it->lock()) {
            if (conn->is_alive()) {
                conn->send_text(message);
                sent_count++;
                ++weak_it;
            } else {
                weak_it = subscribers.erase(weak_it);
            }
        } else {
            weak_it = subscribers.erase(weak_it);
        }
    }
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_messages_sent += sent_count;
    stats_.total_bytes_sent += message.size() * sent_count;
    
    CASHEW_LOG_DEBUG("Broadcast event {} to {} subscribers", 
                     event_type_to_string(event_type), sent_count);
}

void WebSocketHandler::subscribe(std::shared_ptr<WsConnection> conn, WsEventType event_type) {
    if (!conn) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    auto& subscribers = subscriptions_[event_type];
    
    // Check if already subscribed
    for (const auto& weak_conn : subscribers) {
        if (auto existing = weak_conn.lock()) {
            if (existing->id() == conn->id()) {
                return;  // Already subscribed
            }
        }
    }
    
    // Count subscriptions for this connection
    size_t sub_count = 0;
    for (const auto& [type, subs] : subscriptions_) {
        for (const auto& weak_conn : subs) {
            if (auto existing = weak_conn.lock()) {
                if (existing->id() == conn->id()) {
                    sub_count++;
                }
            }
        }
    }
    
    if (sub_count >= config_.max_subscriptions_per_client) {
        CASHEW_LOG_WARN("WS [{}] Max subscriptions reached", conn->id());
        return;
    }
    
    subscribers.push_back(conn);
    stats_mutex_.lock();
    stats_.subscription_count++;
    stats_mutex_.unlock();
    
    CASHEW_LOG_DEBUG("WS [{}] Subscribed to {}", 
                     conn->id(), event_type_to_string(event_type));
}

void WebSocketHandler::unsubscribe(std::shared_ptr<WsConnection> conn, WsEventType event_type) {
    if (!conn) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    auto it = subscriptions_.find(event_type);
    if (it == subscriptions_.end()) {
        return;
    }
    
    auto& subscribers = it->second;
    for (auto sub_it = subscribers.begin(); sub_it != subscribers.end();) {
        if (auto existing = sub_it->lock()) {
            if (existing->id() == conn->id()) {
                sub_it = subscribers.erase(sub_it);
                
                stats_mutex_.lock();
                stats_.subscription_count--;
                stats_mutex_.unlock();
                
                CASHEW_LOG_DEBUG("WS [{}] Unsubscribed from {}", 
                                conn->id(), event_type_to_string(event_type));
                return;
            }
            ++sub_it;
        } else {
            sub_it = subscribers.erase(sub_it);
        }
    }
}

void WebSocketHandler::set_message_handler(WsMessageHandler handler) {
    message_handler_ = std::move(handler);
}

WebSocketHandler::Statistics WebSocketHandler::get_statistics() const {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::mutex> conn_lock(connections_mutex_);
    
    auto stats = stats_;
    stats.active_connections = connections_.size();
    return stats;
}

void WebSocketHandler::keepalive_loop() {
    while (running_) {
        std::this_thread::sleep_for(config_.ping_interval);
        
        if (!running_) {
            break;
        }
        
        // Send pings and check timeouts
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        for (auto it = connections_.begin(); it != connections_.end();) {
            auto& conn = it->second;
            
            if (!conn->is_alive()) {
                it = connections_.erase(it);
                continue;
            }
            
            auto idle_time = conn->time_since_activity();
            
            // Check timeout
            if (idle_time > config_.timeout) {
                CASHEW_LOG_INFO("WS [{}] Connection timeout", conn->id());
                conn->close(1000, "Timeout");
                it = connections_.erase(it);
                continue;
            }
            
            // Send ping
            if (idle_time > config_.ping_interval) {
                conn->send_ping();
            }
            
            ++it;
        }
        
        cleanup_connections();
    }
}

void WebSocketHandler::cleanup_connections() {
    // Cleanup dead connections from subscriptions
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    for (auto& [type, subscribers] : subscriptions_) {
        for (auto it = subscribers.begin(); it != subscribers.end();) {
            if (it->expired()) {
                it = subscribers.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void WebSocketHandler::handle_control_frame(
    std::shared_ptr<WsConnection> conn,
    const WsFrame& frame
) {
    switch (frame.type) {
        case WsMessageType::PING:
            CASHEW_LOG_TRACE("WS [{}] Received ping", conn->id());
            // Send pong in response
            // In real implementation would construct and send pong frame
            break;
            
        case WsMessageType::PONG:
            CASHEW_LOG_TRACE("WS [{}] Received pong", conn->id());
            break;
            
        case WsMessageType::CLOSE: {
            CASHEW_LOG_INFO("WS [{}] Received close", conn->id());
            conn->close(1000, "Client requested close");
            
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.erase(conn->id());
            break;
        }
            
        case WsMessageType::TEXT:
        case WsMessageType::BINARY:
            // Not control frames
            break;
    }
}

// ===== Helper Functions =====

WsEventType parse_event_type(const std::string& type_str) {
    if (type_str == "network_update") return WsEventType::NETWORK_UPDATE;
    if (type_str == "thing_update") return WsEventType::THING_UPDATE;
    if (type_str == "ledger_event") return WsEventType::LEDGER_EVENT;
    if (type_str == "peer_status") return WsEventType::PEER_STATUS;
    if (type_str == "reputation_change") return WsEventType::REPUTATION_CHANGE;
    if (type_str == "gossip_message") return WsEventType::GOSSIP_MESSAGE;
    
    throw std::invalid_argument("Unknown event type: " + type_str);
}

std::string event_type_to_string(WsEventType type) {
    switch (type) {
        case WsEventType::NETWORK_UPDATE: return "network_update";
        case WsEventType::THING_UPDATE: return "thing_update";
        case WsEventType::LEDGER_EVENT: return "ledger_event";
        case WsEventType::PEER_STATUS: return "peer_status";
        case WsEventType::REPUTATION_CHANGE: return "reputation_change";
        case WsEventType::GOSSIP_MESSAGE: return "gossip_message";
        default: return "unknown";
    }
}

} // namespace gateway
} // namespace cashew
