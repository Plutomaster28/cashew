#pragma once

#include "cashew/common.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <optional>
#include <thread>
#include <atomic>
#include <mutex>

namespace cashew {

// Forward declarations
namespace storage { class Storage; }
namespace network { class NetworkRegistry; }
namespace gateway { class ContentRenderer; }

namespace gateway {

/**
 * HTTP request method types
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    HEAD
};

/**
 * HTTP status codes
 */
enum class HttpStatus {
    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    CONFLICT = 409,
    INTERNAL_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    SERVICE_UNAVAILABLE = 503
};

/**
 * HTTP request representation
 */
struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    std::vector<uint8_t> body;
    std::string client_ip;
};

/**
 * HTTP response representation
 */
struct HttpResponse {
    HttpStatus status;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    
    HttpResponse() : status(HttpStatus::OK) {
        headers["Content-Type"] = "application/json";
        headers["Server"] = "Cashew-Gateway/1.0";
    }
    
    void set_json_body(const std::string& json) {
        body.assign(json.begin(), json.end());
        headers["Content-Type"] = "application/json; charset=utf-8";
    }
    
    void set_html_body(const std::string& html) {
        body.assign(html.begin(), html.end());
        headers["Content-Type"] = "text/html; charset=utf-8";
    }
    
    void set_binary_body(const std::vector<uint8_t>& data, const std::string& mime_type) {
        body = data;
        headers["Content-Type"] = mime_type;
    }
};

/**
 * Session information for authenticated users
 */
struct GatewaySession {
    std::string session_id;
    std::optional<PublicKey> user_key;  // Present if authenticated
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_activity;
    bool is_anonymous;
    
    // Session capabilities
    bool can_read{true};   // Always true
    bool can_post{false};  // Requires key or PoW
    bool can_vote{false};  // Requires key
    bool can_host{false};  // Requires key + reputation
};

/**
 * Request handler function type
 */
using RequestHandler = std::function<HttpResponse(const HttpRequest&, GatewaySession&)>;

/**
 * Gateway server configuration
 */
struct GatewayConfig {
    std::string bind_address{"0.0.0.0"};
    uint16_t http_port{8080};
    uint16_t https_port{8443};
    bool enable_tls{false};
    std::string tls_cert_path;
    std::string tls_key_path;
    
    // Session settings
    std::chrono::seconds session_timeout{3600};  // 1 hour
    size_t max_sessions{10000};
    
    // Rate limiting
    size_t max_requests_per_minute{60};
    size_t max_requests_per_hour{1000};
    
    // Content settings
    size_t max_request_body_size{10 * 1024 * 1024};  // 10 MB
    size_t streaming_chunk_size{64 * 1024};  // 64 KB
    
    // CORS settings
    bool enable_cors{true};
    std::string cors_origin{"*"};
    
    // Static file serving
    std::string web_root{"./web"};
    bool enable_directory_listing{false};
};

/**
 * Main gateway server
 * 
 * Bridges HTTP/HTTPS requests from browsers to the Cashew P2P network.
 * Handles authentication, session management, and content delivery.
 */
class GatewayServer {
public:
    /**
     * Construct gateway server
     * @param config Server configuration
     */
    explicit GatewayServer(const GatewayConfig& config);
    
    ~GatewayServer();
    
    /**
     * Set storage backend for content retrieval
     * @param storage Storage backend
     */
    void set_storage(std::shared_ptr<storage::Storage> storage);
    
    /**
     * Set content renderer for HTTP content delivery
     * @param renderer Content renderer
     */
    void set_content_renderer(std::shared_ptr<ContentRenderer> renderer);
    
    /**
     * Set network registry for network data
     * @param registry Network registry
     */
    void set_network_registry(std::shared_ptr<network::NetworkRegistry> registry);
    
    /**
     * Start the gateway server
     * @return true on success
     */
    bool start();
    
    /**
     * Stop the gateway server
     */
    void stop();
    
    /**
     * Check if server is running
     */
    bool is_running() const { return running_; }
    
    /**
     * Register a request handler for a specific path pattern
     * @param method HTTP method
     * @param path_pattern Path pattern (supports wildcards)
     * @param handler Handler function
     */
    void register_handler(
        HttpMethod method,
        const std::string& path_pattern,
        RequestHandler handler
    );
    
    /**
     * Get statistics
     */
    struct Statistics {
        size_t total_requests{0};
        size_t active_sessions{0};
        size_t anonymous_sessions{0};
        size_t authenticated_sessions{0};
        size_t bytes_sent{0};
        size_t bytes_received{0};
        std::chrono::system_clock::time_point started_at;
    };
    
    Statistics get_statistics() const;

private:
    /**
     * Main request processing loop
     */
    void process_requests();
    
    /**
     * Setup HTTP server routes
     */
    void setup_http_routes();
    
    /**
     * Handle incoming HTTP request
     */
    HttpResponse handle_request(const HttpRequest& request);
    
    /**
     * Find matching handler for request
     */
    std::optional<RequestHandler> find_handler(HttpMethod method, const std::string& path) const;
    
    /**
     * Create or retrieve session
     */
    GatewaySession& get_or_create_session(const HttpRequest& request);
    
    /**
     * Validate session
     */
    bool validate_session(const std::string& session_id);
    
    /**
     * Clean up expired sessions
     */
    void cleanup_sessions();
    
    /**
     * Apply rate limiting
     */
    bool check_rate_limit(const std::string& client_ip);
    
    /**
     * Apply CORS headers
     */
    void apply_cors_headers(HttpResponse& response) const;
    
    /**
     * Register default handlers
     */
    void register_default_handlers();
    
    // Handler implementations
    HttpResponse handle_root(const HttpRequest& req, GatewaySession& session);
    HttpResponse handle_health(const HttpRequest& req, GatewaySession& session);
    HttpResponse handle_status(const HttpRequest& req, GatewaySession& session);
    HttpResponse handle_networks(const HttpRequest& req, GatewaySession& session);
    HttpResponse handle_network_detail(const HttpRequest& req, GatewaySession& session);
    HttpResponse handle_thing_content(const HttpRequest& req, GatewaySession& session);
    HttpResponse handle_authenticate(const HttpRequest& req, GatewaySession& session);
    HttpResponse handle_static_file(const HttpRequest& req, GatewaySession& session);
    
    GatewayConfig config_;
    
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    
    // HTTP server (forward declared, defined in cpp)
    class HttpServerImpl;
    std::unique_ptr<HttpServerImpl> http_server_;
    
    // Session management
    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, GatewaySession> sessions_;
    
    // Request routing
    struct RouteKey {
        HttpMethod method;
        std::string path_pattern;
        
        bool operator==(const RouteKey& other) const {
            return method == other.method && path_pattern == other.path_pattern;
        }
    };
    
    struct RouteKeyHash {
        size_t operator()(const RouteKey& key) const {
            return std::hash<int>()(static_cast<int>(key.method)) ^ 
                   std::hash<std::string>()(key.path_pattern);
        }
    };
    
    std::unordered_map<RouteKey, RequestHandler, RouteKeyHash> handlers_;
    
    // Rate limiting
    struct ClientRateInfo {
        std::chrono::system_clock::time_point last_request;
        size_t requests_this_minute{0};
        size_t requests_this_hour{0};
    };
    mutable std::mutex rate_limit_mutex_;
    std::unordered_map<std::string, ClientRateInfo> client_rates_;
    
    // Statistics
    std::shared_ptr<network::NetworkRegistry> network_registry_;
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    
    // Dependencies
    std::shared_ptr<storage::Storage> storage_;
    std::shared_ptr<ContentRenderer> content_renderer_;
};

} // namespace gateway
} // namespace cashew
