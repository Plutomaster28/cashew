#include <iostream>
#include <memory>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <thread>

// Crypto
#include "crypto/blake3.hpp"

// Core components
#include "core/node/node.hpp"
#include "core/ledger/ledger.hpp"

// Storage
#include "storage/storage.hpp"

// Network
#include "network/network.hpp"

// Gateway
#include "cashew/gateway/gateway_server.hpp"
#include "cashew/gateway/websocket_handler.hpp"
#include "cashew/gateway/content_renderer.hpp"

// Utilities
#include "utils/logger.hpp"
#include "utils/config.hpp"
#include "cashew/common.hpp"

// Utility: Convert Hash256 to hex string
std::string hash_to_string(const cashew::Hash256& hash) {
    std::stringstream ss;
    for (const auto& byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        CASHEW_LOG_INFO("Shutdown signal received...");
        g_shutdown_requested = true;
    }
}

int main(int argc, char** argv) {
    try {
        // Install signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        
        // Determine config file path
        std::string config_path = "cashew.conf";
        if (argc > 1) {
            config_path = argv[1];
        }
        
        // Load configuration (or use defaults if file doesn't exist)
        cashew::utils::Config config;
        if (std::filesystem::exists(config_path)) {
            config = cashew::utils::Config::load_from_file(config_path);
        } else {
            // Create default config
            config.set("log_level", "info");
            config.set("log_to_file", false);
            config.set("identity_file", "cashew_identity.dat");
            config.set("identity_password", "");
            config.set("data_dir", "./data");
            config.set("http_port", 8080);
            config.set("web_root", "./web");
        }
        
        // Initialize logging
        auto log_level = config.get_or<std::string>("log_level", "info");
        auto log_to_file = config.get_or<bool>("log_to_file", false);
        cashew::utils::Logger::init(log_level, log_to_file);
        
        CASHEW_LOG_INFO("=================================================");
        CASHEW_LOG_INFO("    Cashew Network Node v{}.{}.{}",
            CASHEW_VERSION_MAJOR,
            CASHEW_VERSION_MINOR,
            CASHEW_VERSION_PATCH
        );
        CASHEW_LOG_INFO("   Freedom over profit. Privacy over surveillance.");
        CASHEW_LOG_INFO("=================================================");
        
        // TODO: Fix Node initialization crash - for now create temp NodeID
        CASHEW_LOG_WARN("Node identity generation disabled for testing");
        CASHEW_LOG_WARN("Creating temporary node ID...");
        
        // Generate temporary node ID for testing
        cashew::Hash256 zero_hash{};
        auto temp_hash_result = cashew::crypto::Blake3::hash(cashew::bytes(zero_hash.begin(), zero_hash.end()));
        cashew::NodeID temp_node_id(temp_hash_result);
        
        CASHEW_LOG_INFO("Temporary Node ID: {}", temp_node_id.to_string());
        
        // ========================================================================
        // INITIALIZE CORE COMPONENTS
        // ========================================================================
        
        // 1. Storage Backend
        CASHEW_LOG_INFO("Initializing storage backend...");
        auto data_dir = config.get_or<std::string>("data_dir", "./data");
        auto storage = std::make_shared<cashew::storage::Storage>(
            std::filesystem::path(data_dir) / "storage"
        );
        CASHEW_LOG_INFO("Storage initialized: {} items, {} bytes",
            storage->item_count(), storage->total_size());
        
        // 2. Ledger
        CASHEW_LOG_INFO("Initializing ledger...");
        auto ledger = std::make_shared<cashew::ledger::Ledger>(temp_node_id);
        CASHEW_LOG_INFO("Ledger initialized: epoch {}, {} events",
            ledger->current_epoch(), ledger->event_count());
        
        // 3. Network Registry
        CASHEW_LOG_INFO("Initializing network registry...");
        auto network_registry = std::make_shared<cashew::network::NetworkRegistry>();
        
        // Try to load existing networks from disk
        std::string networks_dir = data_dir + "/networks";
        if (std::filesystem::exists(networks_dir)) {
            network_registry->load_from_disk(networks_dir);
        }
        
        CASHEW_LOG_INFO("Network registry initialized: {} networks ({} healthy)",
            network_registry->total_network_count(),
            network_registry->healthy_network_count());
        
        // ========================================================================
        // INITIALIZE GATEWAY LAYER
        // ========================================================================
        
        // 4. Content Renderer
        CASHEW_LOG_INFO("Initializing content renderer...");
        cashew::gateway::ContentRendererConfig renderer_config;
        renderer_config.max_cache_size_bytes = 100 * 1024 * 1024;  // 100 MB
        renderer_config.chunk_size = 64 * 1024;  // 64 KB
        renderer_config.enable_range_requests = true;
        
        auto content_renderer = std::make_shared<cashew::gateway::ContentRenderer>(renderer_config);
        
        // Wire content renderer to storage backend
        content_renderer->set_fetch_callback([storage](const cashew::Hash256& hash) -> std::optional<std::vector<uint8_t>> {
            cashew::ContentHash content_hash(hash);
            return storage->get_content(content_hash);
        });
        
        CASHEW_LOG_INFO("Content renderer initialized with storage callback");
        
        // 5. WebSocket Handler
        CASHEW_LOG_INFO("Initializing WebSocket handler...");
        cashew::gateway::WsHandlerConfig ws_config;
        ws_config.ping_interval = std::chrono::seconds(30);
        ws_config.timeout = std::chrono::seconds(300);
        ws_config.max_connections = 1000;
        
        auto websocket_handler = std::make_shared<cashew::gateway::WebSocketHandler>(ws_config);
        websocket_handler->start();
        
        CASHEW_LOG_INFO("WebSocket handler started");
        
        // Wire ledger events to WebSocket broadcast
        ledger->set_event_callback([websocket_handler](const cashew::ledger::LedgerEvent& event) {
            // Serialize event to JSON (simplified)
            std::string event_json = "{\"type\":\"ledger_event\",\"event_id\":\"" + 
                                     hash_to_string(event.event_id) + 
                                     "\",\"timestamp\":" + std::to_string(event.timestamp) + "}";
            
            websocket_handler->broadcast_event(
                cashew::gateway::WsEventType::LEDGER_EVENT,
                event_json
            );
        });
        
        CASHEW_LOG_INFO("Ledger event broadcasting wired to WebSocket");
        
        // 6. Gateway Server
        CASHEW_LOG_INFO("Initializing gateway server...");
        cashew::gateway::GatewayConfig gateway_config;
        gateway_config.bind_address = "0.0.0.0";
        gateway_config.http_port = config.get_or<uint16_t>("http_port", 8080);
        gateway_config.web_root = config.get_or<std::string>("web_root", "./web");
        gateway_config.enable_cors = true;
        gateway_config.max_request_body_size = 10 * 1024 * 1024;  // 10 MB
        
        auto gateway = std::make_shared<cashew::gateway::GatewayServer>(gateway_config);
        
        // Wire all dependencies to gateway
        gateway->set_storage(storage);
        gateway->set_content_renderer(content_renderer);
        gateway->set_network_registry(network_registry);
        
        CASHEW_LOG_INFO("Gateway server configured with all dependencies");
        
        // ========================================================================
        // START SERVICES
        // ========================================================================
        
        CASHEW_LOG_INFO("Starting gateway server on port {}...", gateway_config.http_port);
        if (!gateway->start()) {
            CASHEW_LOG_ERROR("Failed to start gateway server");
            return 1;
        }
        
        CASHEW_LOG_INFO("");
        CASHEW_LOG_INFO(" Cashew node is running!");
        CASHEW_LOG_INFO("");
        CASHEW_LOG_INFO("  Gateway:    http://localhost:{}", gateway_config.http_port);
        CASHEW_LOG_INFO("  WebSocket:  ws://localhost:{}/ws", gateway_config.http_port);
        CASHEW_LOG_INFO("  Web UI:     http://localhost:{}/", gateway_config.http_port);
        CASHEW_LOG_INFO("");
        CASHEW_LOG_INFO("  Node ID:    {}", temp_node_id.to_string());
        CASHEW_LOG_INFO("  Storage:    {} items", storage->item_count());
        CASHEW_LOG_INFO("  Networks:   {}", network_registry->total_network_count());
        CASHEW_LOG_INFO("  Ledger:     {} events", ledger->event_count());
        CASHEW_LOG_INFO("");
        CASHEW_LOG_INFO("Press Ctrl+C to shutdown");
        CASHEW_LOG_INFO("");
        
        // ========================================================================
        // MAIN EVENT LOOP
        // ========================================================================
        
        while (!g_shutdown_requested) {
            // TODO: Add periodic tasks here:
            // - Check network health
            // - Cleanup old cache entries
            // - Save state to disk
            // - Sync with peers
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // ========================================================================
        // GRACEFUL SHUTDOWN
        // ========================================================================
        
        CASHEW_LOG_INFO("Shutting down...");
        
        // Stop gateway server
        CASHEW_LOG_INFO("Stopping gateway server...");
        gateway->stop();
        
        // Stop WebSocket handler
        CASHEW_LOG_INFO("Stopping WebSocket handler...");
        websocket_handler->stop();
        
        // Save network state
        CASHEW_LOG_INFO("Saving network state...");
        std::filesystem::create_directories(networks_dir);
        network_registry->save_to_disk(networks_dir);
        
        // TODO: Re-enable node shutdown when Node is fixed
        // CASHEW_LOG_INFO("Shutting down node...");
        // node->shutdown();
        
        CASHEW_LOG_INFO("");
        CASHEW_LOG_INFO("Node stopped. Goodbye! ");
        CASHEW_LOG_INFO("");
        
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}
