#include "cashew/gateway/gateway_server.hpp"
#include "cashew/gateway/content_renderer.hpp"
#include "../storage/storage.hpp"
#include "../network/network.hpp"
#include "../crypto/random.hpp"
#include "../crypto/blake3.hpp"
#include "../crypto/ed25519.hpp"
#include "../security/content_integrity.hpp"
#include "../utils/logger.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "cashew/third_party/httplib.h"

// Windows defines DELETE as a macro, which conflicts with HttpMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <vector>

namespace cashew {
namespace gateway {

// Wrapper for httplib::Server to avoid exposing it in public header
class GatewayServer::HttpServerImpl {
public:
    httplib::Server server;
};

namespace {

std::string generate_session_id() {
    auto random_bytes = crypto::Random::generate(32);
    auto hash = crypto::Blake3::hash(random_bytes);
    
    std::stringstream ss;
    for (size_t i = 0; i < 16; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(hash[i]);
    }
    return ss.str();
}

[[maybe_unused]] std::string status_to_string(HttpStatus status) {
    switch (status) {
        case HttpStatus::OK: return "200 OK";
        case HttpStatus::CREATED: return "201 Created";
        case HttpStatus::NO_CONTENT: return "204 No Content";
        case HttpStatus::BAD_REQUEST: return "400 Bad Request";
        case HttpStatus::UNAUTHORIZED: return "401 Unauthorized";
        case HttpStatus::FORBIDDEN: return "403 Forbidden";
        case HttpStatus::NOT_FOUND: return "404 Not Found";
        case HttpStatus::METHOD_NOT_ALLOWED: return "405 Method Not Allowed";
        case HttpStatus::CONFLICT: return "409 Conflict";
        case HttpStatus::INTERNAL_ERROR: return "500 Internal Server Error";
        case HttpStatus::NOT_IMPLEMENTED: return "501 Not Implemented";
        case HttpStatus::SERVICE_UNAVAILABLE: return "503 Service Unavailable";
        default: return "500 Internal Server Error";
    }
}

std::string method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::HEAD: return "HEAD";
        default: return "UNKNOWN";
    }
}

bool path_matches_pattern(const std::string& path, const std::string& pattern) {
    // Simple wildcard matching (* matches any segment)
    if (pattern == "*" || pattern == "/*") {
        return true;
    }
    
    if (pattern.find('*') == std::string::npos) {
        return path == pattern;
    }
    
    // Pattern contains wildcard - simple prefix/suffix matching
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.size() - 1);
        return path.find(prefix) == 0;
    }
    
    return path == pattern;
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string mime_from_path(const std::filesystem::path& path) {
    const std::string ext = to_lower_ascii(path.extension().string());
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".wasm") return "application/wasm";
    return "application/octet-stream";
}

std::optional<std::filesystem::path> resolve_web_root(const std::string& configured_root) {
    std::error_code ec;
    std::filesystem::path configured(configured_root);

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(configured);
    candidates.push_back(std::filesystem::current_path(ec) / configured);
    if (configured.filename() != "test-site") {
        candidates.push_back(std::filesystem::current_path(ec) / "test-site");
    }

    // Common launch locations: repo root, build/, build/src/
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        candidates.push_back(cwd / "../test-site");
        candidates.push_back(cwd / "../../test-site");
        candidates.push_back(cwd / "../../../test-site");
    }

    for (const auto& candidate : candidates) {
        ec.clear();
        auto canonical = std::filesystem::weakly_canonical(candidate, ec);
        if (ec || canonical.empty()) {
            continue;
        }
        if (std::filesystem::exists(canonical) && std::filesystem::is_directory(canonical)) {
            return canonical;
        }
    }

    return std::nullopt;
}

} // anonymous namespace

GatewayServer::GatewayServer(const GatewayConfig& config)
    : config_(config)
    , http_server_(std::make_unique<HttpServerImpl>())
{
    stats_.started_at = std::chrono::system_clock::now();
    register_default_handlers();
}

GatewayServer::~GatewayServer() {
    if (running_) {
        stop();
    }
}

void GatewayServer::set_storage(std::shared_ptr<storage::Storage> storage) {
    storage_ = std::move(storage);
    CASHEW_LOG_INFO("Storage backend connected to gateway");
}

void GatewayServer::set_content_renderer(std::shared_ptr<ContentRenderer> renderer) {
    content_renderer_ = std::move(renderer);
    
    // Wire up content renderer's fetch callback to storage
    if (content_renderer_ && storage_) {
        content_renderer_->set_fetch_callback([this](const Hash256& hash) -> std::optional<std::vector<uint8_t>> {
            ContentHash content_hash(hash);
            return storage_->get_content(content_hash);
        });
        CASHEW_LOG_INFO("Content renderer connected to gateway with storage callback");
    } else {
        CASHEW_LOG_WARN("Content renderer set but storage not available");
    }
}

void GatewayServer::set_network_registry(std::shared_ptr<network::NetworkRegistry> registry) {
    network_registry_ = std::move(registry);
    CASHEW_LOG_INFO("Network registry connected to gateway");
}

bool GatewayServer::start() {
    if (running_) {
        CASHEW_LOG_WARN("Gateway server already running");
        return false;
    }
    
    CASHEW_LOG_INFO("Starting gateway server on {}:{}", 
                    config_.bind_address, config_.http_port);
    
    running_ = true;
    
    // Setup HTTP routes
    setup_http_routes();
    
    // Start server in background thread
    server_thread_ = std::thread([this]() {
        CASHEW_LOG_INFO("HTTP server thread starting");
        
        // This blocks until stop() is called
        bool success = http_server_->server.listen(
            config_.bind_address.c_str(), 
            config_.http_port
        );
        
        if (!success && running_) {
            CASHEW_LOG_ERROR("Failed to bind to {}:{}", 
                           config_.bind_address, config_.http_port);
        }
        
        CASHEW_LOG_INFO("HTTP server thread exiting");
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    CASHEW_LOG_INFO("Gateway server started successfully");
    return true;
}

void GatewayServer::stop() {
    if (!running_) {
        return;
    }
    
    CASHEW_LOG_INFO("Stopping gateway server");
    running_ = false;
    
    // Stop HTTP server (this will unblock listen())
    http_server_->server.stop();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    CASHEW_LOG_INFO("Gateway server stopped");
}

void GatewayServer::register_handler(
    HttpMethod method,
    const std::string& path_pattern,
    RequestHandler handler
) {
    RouteKey key{method, path_pattern};
    handlers_[key] = std::move(handler);
    
    CASHEW_LOG_DEBUG("Registered handler: {} {}", 
                     method_to_string(method), path_pattern);
}

void GatewayServer::process_requests() {
    while (running_) {
        // Periodic session cleanup
        cleanup_sessions();
        
        // Sleep between cleanup cycles
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void GatewayServer::setup_http_routes() {
    auto& server = http_server_->server;

    auto forward_request = [this](HttpMethod method, const httplib::Request& req, httplib::Response& res) {
        HttpRequest cashew_req;
        cashew_req.method = method;
        cashew_req.path = req.path;
        cashew_req.client_ip = req.remote_addr;
        cashew_req.body.assign(req.body.begin(), req.body.end());

        for (const auto& [key, value] : req.headers) {
            cashew_req.headers[key] = value;
        }

        for (const auto& [key, value] : req.params) {
            cashew_req.query_params[key] = value;
        }

        auto cashew_res = handle_request(cashew_req);
        res.status = static_cast<int>(cashew_res.status);
        for (const auto& [key, value] : cashew_res.headers) {
            res.set_header(key, value);
        }

        std::string content_type = "application/octet-stream";
        if (auto it = cashew_res.headers.find("Content-Type"); it != cashew_res.headers.end()) {
            content_type = it->second;
        }

        std::string body_str(cashew_res.body.begin(), cashew_res.body.end());
        res.set_content(body_str, content_type);
    };
    
    server.Get(R"(.*)", [forward_request](const httplib::Request& req, httplib::Response& res) {
        forward_request(HttpMethod::GET, req, res);
    });

    server.Post(R"(.*)", [forward_request](const httplib::Request& req, httplib::Response& res) {
        forward_request(HttpMethod::POST, req, res);
    });

    server.Put(R"(.*)", [forward_request](const httplib::Request& req, httplib::Response& res) {
        forward_request(HttpMethod::PUT, req, res);
    });

    server.Delete(R"(.*)", [forward_request](const httplib::Request& req, httplib::Response& res) {
        forward_request(HttpMethod::DELETE, req, res);
    });

    server.Options(R"(.*)", [forward_request](const httplib::Request& req, httplib::Response& res) {
        forward_request(HttpMethod::OPTIONS, req, res);
    });
    
    // Catch-all for 404
    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"error": "Not found"})", "application/json");
    });
    
    CASHEW_LOG_INFO("HTTP routes configured");
}

HttpResponse GatewayServer::handle_request(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_requests++;
    stats_.bytes_received += request.body.size();
    
    // Check rate limiting
    if (!check_rate_limit(request.client_ip)) {
        HttpResponse response;
        response.status = HttpStatus::SERVICE_UNAVAILABLE;
        response.set_json_body(R"({"error": "Rate limit exceeded"})");
        apply_cors_headers(response);
        return response;
    }
    
    // Get or create session
    auto& session = get_or_create_session(request);
    
    // Find handler
    auto handler_opt = find_handler(request.method, request.path);
    if (!handler_opt) {
        if (request.method == HttpMethod::GET && request.path.rfind("/api/", 0) != 0) {
            HttpRequest static_req = request;
            if (static_req.path.rfind("/static/", 0) != 0) {
                if (!static_req.path.empty() && static_req.path.front() == '/') {
                    static_req.path = "/static" + static_req.path;
                } else {
                    static_req.path = "/static/" + static_req.path;
                }
            }

            auto response = handle_static_file(static_req, session);
            apply_cors_headers(response);
            stats_.bytes_sent += response.body.size();
            return response;
        }

        HttpResponse response;
        response.status = HttpStatus::NOT_FOUND;
        response.set_json_body(R"({"error": "Not found"})");
        apply_cors_headers(response);
        return response;
    }
    
    // Execute handler
    try {
        auto response = (*handler_opt)(request, session);
        apply_cors_headers(response);
        stats_.bytes_sent += response.body.size();
        return response;
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Handler error: {}", e.what());
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body(R"({"error": "Internal server error"})");
        apply_cors_headers(response);
        return response;
    }
}

std::optional<RequestHandler> GatewayServer::find_handler(
    HttpMethod method, 
    const std::string& path
) const {
    // Try exact match first
    RouteKey key{method, path};
    auto it = handlers_.find(key);
    if (it != handlers_.end()) {
        return it->second;
    }
    
    // Try pattern matching
    for (const auto& [route_key, handler] : handlers_) {
        if (route_key.method == method && 
            path_matches_pattern(path, route_key.path_pattern)) {
            return handler;
        }
    }
    
    return std::nullopt;
}

GatewaySession& GatewayServer::get_or_create_session(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Try to find session from cookie
    auto cookie_it = request.headers.find("Cookie");
    if (cookie_it != request.headers.end()) {
        // Parse session ID from cookie (simplified)
        auto& cookie_str = cookie_it->second;
        auto pos = cookie_str.find("cashew_session=");
        if (pos != std::string::npos) {
            auto start = pos + 15;  // Length of "cashew_session="
            auto end = cookie_str.find(';', start);
            auto session_id = cookie_str.substr(start, 
                end == std::string::npos ? std::string::npos : end - start);
            
            auto session_it = sessions_.find(session_id);
            if (session_it != sessions_.end()) {
                session_it->second.last_activity = std::chrono::system_clock::now();
                return session_it->second;
            }
        }
    }
    
    // Create new session
    if (sessions_.size() >= config_.max_sessions) {
        // Clean up oldest sessions
        cleanup_sessions();
    }
    
    GatewaySession new_session;
    new_session.session_id = generate_session_id();
    new_session.created_at = std::chrono::system_clock::now();
    new_session.last_activity = new_session.created_at;
    new_session.is_anonymous = true;
    
    auto [it, inserted] = sessions_.emplace(new_session.session_id, new_session);
    return it->second;
}

bool GatewayServer::validate_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - it->second.last_activity
    );
    
    if (elapsed > config_.session_timeout) {
        sessions_.erase(it);
        return false;
    }
    
    return true;
}

void GatewayServer::cleanup_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto now = std::chrono::system_clock::now();
    
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_activity
        );
        
        if (elapsed > config_.session_timeout) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

bool GatewayServer::check_rate_limit(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto& rate_info = client_rates_[client_ip];
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - rate_info.last_request
    );
    
    // Reset counters if enough time has passed
    if (elapsed.count() >= 60) {
        rate_info.requests_this_minute = 0;
    }
    if (elapsed.count() >= 3600) {
        rate_info.requests_this_hour = 0;
    }
    
    rate_info.requests_this_minute++;
    rate_info.requests_this_hour++;
    rate_info.last_request = now;
    
    return rate_info.requests_this_minute <= config_.max_requests_per_minute &&
           rate_info.requests_this_hour <= config_.max_requests_per_hour;
}

void GatewayServer::apply_cors_headers(HttpResponse& response) const {
    if (config_.enable_cors) {
        response.headers["Access-Control-Allow-Origin"] = config_.cors_origin;
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        response.headers["Access-Control-Max-Age"] = "86400";
    }
}

void GatewayServer::register_default_handlers() {
    using namespace std::placeholders;
    
    // Root endpoint
    register_handler(HttpMethod::GET, "/", 
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_root(req, session);
        });
    
    // Health check
    register_handler(HttpMethod::GET, "/health",
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_health(req, session);
        });
    
    // Status endpoint
    register_handler(HttpMethod::GET, "/api/status",
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_status(req, session);
        });
    
    // Network listing
    register_handler(HttpMethod::GET, "/api/networks",
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_networks(req, session);
        });
    
    // Network detail
    register_handler(HttpMethod::GET, "/api/networks/*",
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_network_detail(req, session);
        });
    
    // Thing content
    register_handler(HttpMethod::GET, "/api/thing/*",
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_thing_content(req, session);
        });
    
    // Authentication
    register_handler(HttpMethod::POST, "/api/auth",
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_authenticate(req, session);
        });
    
    // Static file serving
    register_handler(HttpMethod::GET, "/static/*",
        [this](const HttpRequest& req, GatewaySession& session) {
            return handle_static_file(req, session);
        });
}

HttpResponse GatewayServer::handle_root(const HttpRequest& /* req */, GatewaySession& session) {
    const auto resolved_root = resolve_web_root(config_.web_root);
    if (!resolved_root) {
        CASHEW_LOG_WARN("Static web root not found: {}", config_.web_root);
    }
    const auto index_path = (resolved_root ? *resolved_root : std::filesystem::path(config_.web_root)) / "index.html";
    if (std::filesystem::exists(index_path) && std::filesystem::is_regular_file(index_path)) {
        HttpRequest static_req;
        static_req.method = HttpMethod::GET;
        static_req.path = "/static/index.html";
        return handle_static_file(static_req, session);
    }

    HttpResponse response;
    response.set_html_body(R"(
<!DOCTYPE html>
<html>
<head>
    <title>Cashew Network Gateway</title>
    <meta charset="utf-8">
    <style>
        body { font-family: sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
        h1 { color: #333; }
        .status { color: #28a745; font-weight: bold; }
        a { color: #007bff; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <h1>Cashew Network Gateway</h1>
    <p class="status">Status: Online</p>
    <p>Welcome to the Cashew decentralized network! This gateway provides browser access to the P2P network.</p>
    <h2>API Endpoints</h2>
    <ul>
        <li><a href="/health">/health</a> - Server health check</li>
        <li><a href="/api/status">/api/status</a> - Network status</li>
        <li><a href="/api/networks">/api/networks</a> - List available networks</li>
        <li>/api/thing/:hash - Retrieve content by hash</li>
        <li>/api/auth - Authenticate with key (POST)</li>
    </ul>
    <p><small>Powered by Cashew Network v1.0</small></p>
</body>
</html>
    )");
    return response;
}

HttpResponse GatewayServer::handle_health(const HttpRequest& /* req */, GatewaySession& /* session */) {
    HttpResponse response;
    response.set_json_body(R"({"status": "healthy", "service": "cashew-gateway"})");
    return response;
}

HttpResponse GatewayServer::handle_status(const HttpRequest& /* req */, GatewaySession& /* session */) {
    auto stats = get_statistics();
    
    std::stringstream json;
    json << "{";
    json << R"("service": "cashew-gateway",)";
    json << R"("total_requests": )" << stats.total_requests << ",";
    json << R"("active_sessions": )" << stats.active_sessions << ",";
    json << R"("anonymous_sessions": )" << stats.anonymous_sessions << ",";
    json << R"("authenticated_sessions": )" << stats.authenticated_sessions << ",";
    json << R"("bytes_sent": )" << stats.bytes_sent << ",";
    json << R"("bytes_received": )" << stats.bytes_received;
    json << "}";
    
    HttpResponse response;
    response.set_json_body(json.str());
    return response;
}

HttpResponse GatewayServer::handle_networks(const HttpRequest& /* req */, GatewaySession& /* session */) {
    if (!network_registry_) {
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body("{\"error\": \"Network registry not available\"}");
        return response;
    }
    
    auto networks = network_registry_->get_all_networks();
    
    std::stringstream json;
    json << "{\"networks\": [";
    
    bool first = true;
    for (const auto& network : networks) {
        if (!first) json << ",";
        first = false;
        
        auto network_id = network.get_id();
        auto thing_hash = network.get_thing_hash();
        auto health = network.get_health();
        
        // Convert network ID to hex string
        std::stringstream id_hex;
        for (const auto& byte : network_id.id) {
            id_hex << std::hex << std::setw(2) << std::setfill('0') 
                   << static_cast<int>(byte);
        }
        
        // Convert thing hash to hex string
        std::stringstream hash_hex;
        for (const auto& byte : thing_hash.hash) {
            hash_hex << std::hex << std::setw(2) << std::setfill('0') 
                     << static_cast<int>(byte);
        }
        
        json << "{";
        json << "\"id\": \"" << id_hex.str() << "\",";
        json << "\"thing_hash\": \"" << hash_hex.str() << "\",";
        json << "\"member_count\": " << network.member_count() << ",";
        json << "\"replica_count\": " << network.active_replica_count() << ",";
        json << "\"is_healthy\": " << (network.is_healthy() ? "true" : "false") << ",";
        json << "\"health\": ";
        
        switch (health) {
            case network::NetworkHealth::CRITICAL:
                json << "\"critical\"";
                break;
            case network::NetworkHealth::DEGRADED:
                json << "\"degraded\"";
                break;
            case network::NetworkHealth::HEALTHY:
                json << "\"healthy\"";
                break;
            case network::NetworkHealth::OPTIMAL:
                json << "\"optimal\"";
                break;
        }
        
        json << "}";
    }
    
    json << "],";
    json << "\"total_count\": " << networks.size() << ",";
    json << "\"healthy_count\": " << network_registry_->healthy_network_count();
    json << "}";
    
    HttpResponse response;
    response.set_json_body(json.str());
    return response;
}

HttpResponse GatewayServer::handle_network_detail(const HttpRequest& req, GatewaySession& /* session */) {
    if (!network_registry_) {
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body("{\"error\": \"Network registry not available\"}");
        return response;
    }
    
    // Extract network ID from path ("/api/networks/" is 14 chars)
    if (req.path.size() <= 14) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Missing network ID\"}");
        return response;
    }
    
    auto id_str = req.path.substr(14);
    
    // Parse network ID from hex string (32 bytes = 64 hex chars)
    if (id_str.size() != 64) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Invalid network ID format\"}");
        return response;
    }
    
    network::NetworkID network_id;
    try {
        for (size_t i = 0; i < 32; ++i) {
            network_id.id[i] = static_cast<uint8_t>(
                std::stoi(id_str.substr(i * 2, 2), nullptr, 16)
            );
        }
    } catch (...) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Invalid network ID encoding\"}");
        return response;
    }
    
    auto network_opt = network_registry_->get_network(network_id);
    if (!network_opt) {
        HttpResponse response;
        response.status = HttpStatus::NOT_FOUND;
        response.set_json_body("{\"error\": \"Network not found\"}");
        return response;
    }
    
    const auto& network = *network_opt;
    auto members = network.get_all_members();
    auto quorum = network.get_quorum();
    
    std::stringstream json;
    json << "{";
    json << "\"id\": \"" << id_str << "\",";
    
    // Thing hash
    std::stringstream hash_hex;
    for (const auto& byte : network.get_thing_hash().hash) {
        hash_hex << std::hex << std::setw(2) << std::setfill('0') 
                 << static_cast<int>(byte);
    }
    json << "\"thing_hash\": \"" << hash_hex.str() << "\",";
    
    // Health
    json << "\"is_healthy\": " << (network.is_healthy() ? "true" : "false") << ",";
    json << "\"member_count\": " << network.member_count() << ",";
    json << "\"replica_count\": " << network.active_replica_count() << ",";
    
    // Quorum settings
    json << "\"quorum\": {";
    json << "\"min_replicas\": " << quorum.min_replicas << ",";
    json << "\"target_replicas\": " << quorum.target_replicas << ",";
    json << "\"max_replicas\": " << quorum.max_replicas;
    json << "},";
    
    // Members
    json << "\"members\": [";
    bool first = true;
    for (const auto& member : members) {
        if (!first) json << ",";
        first = false;
        
        json << "{";
        json << "\"node_id\": \"" << member.node_id.to_string() << "\",";
        json << "\"role\": ";
        switch (member.role) {
            case network::MemberRole::FOUNDER:
                json << "\"founder\"";
                break;
            case network::MemberRole::FULL:
                json << "\"full\"";
                break;
            case network::MemberRole::PENDING:
                json << "\"pending\"";
                break;
            case network::MemberRole::OBSERVER:
                json << "\"observer\"";
                break;
        }
        json << ",";
        json << "\"has_replica\": " << (member.has_complete_replica ? "true" : "false") << ",";
        json << "\"reliability\": " << member.reliability_score;
        json << "}";
    }
    json << "]";
    
    json << "}";
    
    HttpResponse response;
    response.set_json_body(json.str());
    return response;
}

HttpResponse GatewayServer::handle_thing_content(const HttpRequest& req, GatewaySession& session) {
    if (!session.can_read) {
        HttpResponse response;
        response.status = HttpStatus::FORBIDDEN;
        response.set_json_body(R"({"error": "Read access denied"})");
        return response;
    }
    
    // Extract thing hash from path ("/api/thing/" is 11 chars, not 12)
    if (req.path.size() <= 11) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body(R"({"error": "Missing content hash"})");
        return response;
    }
    
    auto hash_str = req.path.substr(11);  // After "/api/thing/"
    
    // Parse hash from hex string
    if (hash_str.size() != 64) {  // BLAKE3 = 32 bytes = 64 hex chars
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Invalid hash format (expected 64 hex characters)\"}");
        return response;
    }
    
    Hash256 content_hash;
    try {
        for (size_t i = 0; i < 32; ++i) {
            content_hash[i] = static_cast<uint8_t>(
                std::stoi(hash_str.substr(i * 2, 2), nullptr, 16)
            );
        }
    } catch (...) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body(R"({"error": "Invalid hash encoding"})");
        return response;
    }
    
    // Check if content renderer is available
    if (!content_renderer_) {
        CASHEW_LOG_ERROR("Content renderer not initialized");
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body(R"({"error": "Content renderer not available"})");
        return response;
    }
    
    // Try to render content
    auto render_result = content_renderer_->render_content(content_hash);
    if (!render_result) {
        CASHEW_LOG_WARN("Content not found: {}", hash_str);
        HttpResponse response;
        response.status = HttpStatus::NOT_FOUND;
        response.set_json_body(R"({"error": "Content not found"})");
        return response;
    }
    
    // Verify content integrity before serving
    auto integrity_result = security::ContentIntegrityChecker::verify_content(
        render_result->data,
        content_hash
    );
    
    if (!integrity_result.is_valid) {
        CASHEW_LOG_ERROR("Content integrity verification failed for {}: {}", 
                        hash_str, integrity_result.error_message);
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body(R"({"error": "Content integrity verification failed"})");
        return response;
    }
    
    CASHEW_LOG_DEBUG("Content integrity verified: {} ({} bytes)", hash_str, integrity_result.content_size);
    
    // Build HTTP response from render result
    HttpResponse response;
    response.status = HttpStatus::OK;
    response.body = std::move(render_result->data);
    std::string resolved_mime = render_result->metadata.mime_type;
    if (storage_) {
        if (auto stored_mime = storage_->get_metadata("mime_" + hash_str); stored_mime && !stored_mime->empty()) {
            resolved_mime.assign(stored_mime->begin(), stored_mime->end());
        }
    }
    response.headers["Content-Type"] = resolved_mime;
    response.headers["Content-Length"] = std::to_string(response.body.size());
    
    // Add cache headers
    if (render_result->metadata.is_cacheable) {
        response.headers["Cache-Control"] = "public, max-age=3600";
        response.headers["ETag"] = "\"" + hash_str + "\"";
    } else {
        response.headers["Cache-Control"] = "no-cache, no-store, must-revalidate";
    }
    
    CASHEW_LOG_DEBUG("Served content: {} ({} bytes)", hash_str, response.body.size());
    
    return response;
}

HttpResponse GatewayServer::handle_authenticate(const HttpRequest& req, GatewaySession& session) {
    // Parse JSON request body
    if (req.body.empty()) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Empty request body\"}");
        return response;
    }
    
    std::string body_str(req.body.begin(), req.body.end());
    
    // Very simple JSON parsing (production would use nlohmann::json)
    // Expected: {"public_key": "hex", "message": "challenge", "signature": "hex"}
    auto find_field = [&body_str](const std::string& field) -> std::string {
        auto field_key = "\"" + field + "\":";
        auto pos = body_str.find(field_key);
        if (pos == std::string::npos) return "";
        
        pos += field_key.length();
        while (pos < body_str.length() && (body_str[pos] == ' ' || body_str[pos] == '\"')) {
            ++pos;
        }
        
        auto end_pos = pos;
        while (end_pos < body_str.length() && body_str[end_pos] != '\"' && body_str[end_pos] != ',') {
            ++end_pos;
        }
        
        return body_str.substr(pos, end_pos - pos);
    };
    
    auto public_key_hex = find_field("public_key");
    auto message_str = find_field("message");
    auto signature_hex = find_field("signature");
    
    if (public_key_hex.empty() || message_str.empty() || signature_hex.empty()) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Missing required fields (public_key, message, signature)\"}");
        return response;
    }
    
    // Parse public key from hex
    auto public_key_opt = crypto::Ed25519::public_key_from_hex(public_key_hex);
    if (!public_key_opt) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Invalid public key format\"}");
        return response;
    }
    
    // Parse signature from hex
    auto signature_opt = crypto::Ed25519::signature_from_hex(signature_hex);
    if (!signature_opt) {
        HttpResponse response;
        response.status = HttpStatus::BAD_REQUEST;
        response.set_json_body("{\"error\": \"Invalid signature format\"}");
        return response;
    }
    
    // Convert message string to bytes
    bytes message(message_str.begin(), message_str.end());
    
    // Verify signature
    if (!crypto::Ed25519::verify(message, *signature_opt, *public_key_opt)) {
        CASHEW_LOG_WARN("Authentication failed: invalid signature from {}", public_key_hex.substr(0, 16));
        HttpResponse response;
        response.status = HttpStatus::UNAUTHORIZED;
        response.set_json_body("{\"error\": \"Invalid signature\"}");
        return response;
    }
    
    // Authentication successful - upgrade session capabilities
    session.user_key = *public_key_opt;
    session.is_anonymous = false;
    session.can_post = true;
    session.can_vote = true;
    // can_host requires additional reputation check (future enhancement)
    
    CASHEW_LOG_INFO("User authenticated: {}", public_key_hex.substr(0, 16));
    
    std::stringstream json;
    json << "{";
    json << "\"authenticated\": true,";
    json << "\"session_id\": \"" << session.session_id << "\",";
    json << "\"capabilities\": {";
    json << "\"can_read\": true,";
    json << "\"can_post\": true,";
    json << "\"can_vote\": true,";
    json << "\"can_host\": " << (session.can_host ? "true" : "false");
    json << "}";
    json << "}";
    
    HttpResponse response;
    response.set_json_body(json.str());
    return response;
}

HttpResponse GatewayServer::handle_static_file(const HttpRequest& req, GatewaySession& /* session */) {
    const auto root_canonical_opt = resolve_web_root(config_.web_root);
    if (!root_canonical_opt) {
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body(R"({"error": "Invalid web root configuration"})");
        return response;
    }
    std::error_code ec;
    const std::filesystem::path root_canonical = *root_canonical_opt;

    std::string relative = req.path;
    if (relative.rfind("/static/", 0) == 0) {
        relative = relative.substr(8);
    }

    if (relative.empty() || relative == "/") {
        relative = "index.html";
    }

    while (!relative.empty() && relative.front() == '/') {
        relative.erase(relative.begin());
    }

    std::filesystem::path requested = root_canonical / std::filesystem::path(relative);
    std::filesystem::path resolved = std::filesystem::weakly_canonical(requested, ec);
    if (ec || resolved.empty()) {
        HttpResponse response;
        response.status = HttpStatus::NOT_FOUND;
        response.set_json_body(R"({"error": "File not found"})");
        return response;
    }

    const auto relative_to_root = resolved.lexically_relative(root_canonical);
    if (relative_to_root.empty() || relative_to_root.string().rfind("..", 0) == 0) {
        HttpResponse response;
        response.status = HttpStatus::FORBIDDEN;
        response.set_json_body(R"({"error": "Path outside web root"})");
        return response;
    }

    const std::string rel_lower = to_lower_ascii(relative_to_root.generic_string());
    const std::string ext_lower = to_lower_ascii(resolved.extension().string());
    if (rel_lower.find("/cgi-bin/") != std::string::npos ||
        ext_lower == ".pl" || ext_lower == ".cgi" || ext_lower == ".pm") {
        HttpResponse response;
        response.status = HttpStatus::FORBIDDEN;
        response.set_json_body(R"({"error": "Executable scripts are not served by static gateway"})");
        return response;
    }

    if (std::filesystem::is_directory(resolved)) {
        if (!config_.enable_directory_listing) {
            auto index_path = resolved / "index.html";
            if (std::filesystem::exists(index_path) && std::filesystem::is_regular_file(index_path)) {
                resolved = index_path;
            } else {
                HttpResponse response;
                response.status = HttpStatus::NOT_FOUND;
                response.set_json_body(R"({"error": "Directory listing disabled"})");
                return response;
            }
        } else {
            std::ostringstream html;
            html << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Index of /static/"
                 << relative << "</title></head><body>";
            html << "<h1>Index of /static/" << relative << "</h1><ul>";
            for (const auto& entry : std::filesystem::directory_iterator(resolved)) {
                html << "<li>" << entry.path().filename().string() << "</li>";
            }
            html << "</ul></body></html>";

            HttpResponse response;
            response.status = HttpStatus::OK;
            response.set_html_body(html.str());
            return response;
        }
    }

    if (!std::filesystem::exists(resolved) || !std::filesystem::is_regular_file(resolved)) {
        HttpResponse response;
        response.status = HttpStatus::NOT_FOUND;
        response.set_json_body(R"({"error": "File not found"})");
        return response;
    }

    std::ifstream file(resolved, std::ios::binary | std::ios::ate);
    if (!file) {
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body(R"({"error": "Failed to open file"})");
        return response;
    }

    const auto size = file.tellg();
    if (size < 0) {
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body(R"({"error": "Invalid file size"})");
        return response;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) {
        HttpResponse response;
        response.status = HttpStatus::INTERNAL_ERROR;
        response.set_json_body(R"({"error": "Failed to read file"})");
        return response;
    }

    HttpResponse response;
    response.status = HttpStatus::OK;
    response.set_binary_body(data, mime_from_path(resolved));
    response.headers["Content-Length"] = std::to_string(data.size());
    response.headers["Cache-Control"] = "public, max-age=300";
    return response;
}

GatewayServer::Statistics GatewayServer::get_statistics() const {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::mutex> session_lock(sessions_mutex_);
    
    auto stats = stats_;
    stats.active_sessions = sessions_.size();
    
    for (const auto& [id, session] : sessions_) {
        if (session.is_anonymous) {
            stats.anonymous_sessions++;
        } else {
            stats.authenticated_sessions++;
        }
    }
    
    return stats;
}

} // namespace gateway
} // namespace cashew
