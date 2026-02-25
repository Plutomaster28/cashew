#include "gateway/gateway_server.hpp"
#include "gateway/websocket_handler.hpp"
#include "gateway/content_renderer.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::gateway;

class GatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Gateway tests
    }
};

// ============================================================================
// HTTP Gateway Tests
// ============================================================================

TEST_F(GatewayTest, GatewayInitialization) {
    GatewayConfig config;
    config.port = 8080;
    config.enable_https = false;
    
    GatewayServer server(config);
    
    EXPECT_EQ(server.port(), 8080);
    EXPECT_FALSE(server.is_https_enabled());
}

TEST_F(GatewayTest, RouteRegistration) {
    GatewayConfig config{8080, false};
    GatewayServer server(config);
    
    // Register routes
    server.register_route("/api/test", HTTPMethod::GET, [](const Request& req) {
        Response res;
        res.status = 200;
        res.body = "test response";
        return res;
    });
    
    EXPECT_TRUE(server.has_route("/api/test"));
}

TEST_F(GatewayTest, APIEndpoints) {
    GatewayConfig config{8080, false};
    GatewayServer server(config);
    
    server.setup_default_routes();
    
    // Check standard endpoints
    EXPECT_TRUE(server.has_route("/api/thing"));
    EXPECT_TRUE(server.has_route("/api/identity"));
    EXPECT_TRUE(server.has_route("/api/network"));
}

TEST_F(GatewayTest, SessionManagement) {
    SessionManager manager;
    
    // Create session
    auto session_id = manager.create_session("user123");
    EXPECT_GT(session_id.size(), 0);
    
    // Validate session
    EXPECT_TRUE(manager.is_valid(session_id));
    
    // Destroy session
    manager.destroy_session(session_id);
    EXPECT_FALSE(manager.is_valid(session_id));
}

TEST_F(GatewayTest, APIAuthentication) {
    AuthManager auth;
    
    // Create API key
    auto api_key = auth.create_api_key("user123", {"read", "write"});
    EXPECT_GT(api_key.size(), 0);
    
    // Verify API key
    auto permissions = auth.verify_api_key(api_key);
    ASSERT_TRUE(permissions.has_value());
    EXPECT_EQ(permissions->size(), 2);
    
    // Invalid key
    auto invalid = auth.verify_api_key("invalid_key");
    EXPECT_FALSE(invalid.has_value());
}

TEST_F(GatewayTest, ContentStreaming) {
    ContentStreamer streamer;
    
    // Large content
    bytes content(10 * 1024 * 1024, 0x42); // 10MB
    
    // Stream in chunks
    size_t chunk_size = 1024 * 1024; // 1MB chunks
    size_t offset = 0;
    int chunk_count = 0;
    
    while (offset < content.size()) {
        auto chunk = streamer.get_chunk(content, offset, chunk_size);
        EXPECT_LE(chunk.size(), chunk_size);
        offset += chunk.size();
        chunk_count++;
    }
    
    EXPECT_EQ(chunk_count, 10);
}

TEST_F(GatewayTest, HTTPSSupport) {
    GatewayConfig config;
    config.port = 8443;
    config.enable_https = true;
    config.cert_file = "test.crt";
    config.key_file = "test.key";
    
    GatewayServer server(config);
    
    EXPECT_TRUE(server.is_https_enabled());
}

// ============================================================================
// WebSocket Tests
// ============================================================================

TEST_F(GatewayTest, WebSocketConnection) {
    WebSocketHandler handler;
    
    // Simulate connection
    auto conn_id = handler.create_connection();
    EXPECT_GT(conn_id.size(), 0);
    
    EXPECT_TRUE(handler.is_connected(conn_id));
    
    // Close connection
    handler.close_connection(conn_id);
    EXPECT_FALSE(handler.is_connected(conn_id));
}

TEST_F(GatewayTest, WebSocketMessaging) {
    WebSocketHandler handler;
    
    auto conn_id = handler.create_connection();
    
    // Send message
    std::string msg = "test message";
    bool sent = handler.send_message(conn_id, msg);
    EXPECT_TRUE(sent);
}

TEST_F(GatewayTest, WebSocketBroadcast) {
    WebSocketHandler handler;
    
    // Create multiple connections
    auto conn1 = handler.create_connection();
    auto conn2 = handler.create_connection();
    auto conn3 = handler.create_connection();
    
    // Broadcast message
    std::string msg = "broadcast test";
    int delivered = handler.broadcast(msg);
    
    EXPECT_EQ(delivered, 3);
}

TEST_F(GatewayTest, RealtimeUpdates) {
    WebSocketHandler handler;
    UpdateManager updates;
    
    auto conn_id = handler.create_connection();
    
    // Subscribe to updates
    handler.subscribe(conn_id, "ledger");
    handler.subscribe(conn_id, "network");
    
    // Trigger update
    updates.notify("ledger", {{"height", 100}});
    
    // Connection should receive update
    auto pending = handler.get_pending_updates(conn_id);
    EXPECT_GT(pending.size(), 0);
}

TEST_F(GatewayTest, WebSocketAuthentication) {
    WebSocketHandler handler;
    AuthManager auth;
    
    auto conn_id = handler.create_connection();
    
    // Authenticate connection
    std::string token = "valid_token";
    bool authenticated = handler.authenticate(conn_id, token);
    
    EXPECT_TRUE(authenticated);
    EXPECT_TRUE(handler.is_authenticated(conn_id));
}

// ============================================================================
// Content Rendering Tests
// ============================================================================

TEST_F(GatewayTest, HTMLRendering) {
    ContentRenderer renderer;
    
    bytes content = {'H', 'e', 'l', 'l', 'o'};
    
    auto html = renderer.render_as_html(content, "text/plain");
    EXPECT_GT(html.size(), 0);
    EXPECT_TRUE(html.find("Hello") != std::string::npos);
}

TEST_F(GatewayTest, ContentTypeDetection) {
    ContentRenderer renderer;
    
    // HTML content
    bytes html_content = {'<', 'h', 't', 'm', 'l', '>'};
    auto type1 = renderer.detect_content_type(html_content);
    EXPECT_EQ(type1, "text/html");
    
    // JSON content
    bytes json_content = {'{', '"', 'a', '"', ':', '1', '}'};
    auto type2 = renderer.detect_content_type(json_content);
    EXPECT_EQ(type2, "application/json");
}

TEST_F(GatewayTest, ImageRendering) {
    ContentRenderer renderer;
    
    // Fake PNG header
    bytes png_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    
    auto html = renderer.render_as_html(png_data, "image/png");
    
    EXPECT_TRUE(html.find("<img") != std::string::npos);
}

TEST_F(GatewayTest, CodeHighlighting) {
    ContentRenderer renderer;
    
    std::string code = "int main() { return 0; }";
    bytes code_bytes(code.begin(), code.end());
    
    auto html = renderer.render_as_html(code_bytes, "text/x-c++");
    
    // Should have syntax highlighting
    EXPECT_GT(html.size(), code.size());
}

// ============================================================================
// Security Tests
// ============================================================================

TEST_F(GatewayTest, CSPHeaders) {
    GatewayServer server({8080, false});
    
    auto headers = server.get_security_headers();
    
    EXPECT_TRUE(headers.contains("Content-Security-Policy"));
    EXPECT_TRUE(headers["Content-Security-Policy"].find("default-src 'self'") != std::string::npos);
}

TEST_F(GatewayTest, CORSConfiguration) {
    GatewayServer server({8080, false});
    
    CORSConfig cors;
    cors.allowed_origins = {"http://localhost:3000"};
    cors.allowed_methods = {"GET", "POST"};
    
    server.configure_cors(cors);
    
    auto headers = server.get_cors_headers("http://localhost:3000");
    EXPECT_TRUE(headers.contains("Access-Control-Allow-Origin"));
}

TEST_F(GatewayTest, RequestSandboxing) {
    RequestSandbox sandbox;
    
    Request req;
    req.path = "/api/thing";
    req.method = HTTPMethod::GET;
    
    // Valid request
    EXPECT_TRUE(sandbox.is_safe(req));
    
    // Path traversal attempt
    req.path = "/api/../../etc/passwd";
    EXPECT_FALSE(sandbox.is_safe(req));
    
    // SQL injection attempt
    req.params["id"] = "1; DROP TABLE users--";
    EXPECT_FALSE(sandbox.is_safe(req));
}

TEST_F(GatewayTest, RateLimiting) {
    GatewayRateLimiter limiter(10, std::chrono::seconds(1));
    
    std::string client_ip = "192.168.1.100";
    
    // Allow first 10
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(limiter.allow(client_ip));
    }
    
    // Block 11th
    EXPECT_FALSE(limiter.allow(client_ip));
}

TEST_F(GatewayTest, XSSPrevention) {
    HTMLSanitizer sanitizer;
    
    // Malicious script
    std::string malicious = "<script>alert('XSS')</script>Hello";
    auto sanitized = sanitizer.sanitize(malicious);
    
    // Script should be removed
    EXPECT_TRUE(sanitized.find("<script>") == std::string::npos);
    EXPECT_TRUE(sanitized.find("Hello") != std::string::npos);
    
    // Allowed HTML
    std::string safe = "<p>Hello <b>World</b></p>";
    auto safe_result = sanitizer.sanitize(safe);
    EXPECT_TRUE(safe_result.find("<p>") != std::string::npos);
}

TEST_F(GatewayTest, ContentIntegrityVerification) {
    GatewayServer server({8080, false});
    
    bytes content = {1, 2, 3, 4, 5};
    Hash256 expected_hash;
    // Calculate expected hash
    
    // Verify content matches hash
    bool valid = server.verify_content_integrity(content, expected_hash);
    
    // Should implement proper verification
}

TEST_F(GatewayTest, ErrorHandling) {
    GatewayServer server({8080, false});
    
    // 404 Not Found
    auto resp404 = server.handle_error(404, "Not Found");
    EXPECT_EQ(resp404.status, 404);
    EXPECT_TRUE(resp404.body.find("Not Found") != std::string::npos);
    
    // 500 Internal Server Error
    auto resp500 = server.handle_error(500, "Internal Error");
    EXPECT_EQ(resp500.status, 500);
}

TEST_F(GatewayTest, RequestLogging) {
    RequestLogger logger;
    
    Request req;
    req.path = "/api/thing";
    req.method = HTTPMethod::GET;
    req.client_ip = "192.168.1.100";
    
    Response resp;
    resp.status = 200;
    
    logger.log(req, resp);
    
    auto entries = logger.get_recent(10);
    EXPECT_GT(entries.size(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
