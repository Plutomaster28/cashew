#include <iostream>
#include <memory>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <thread>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <vector>

// Crypto
#include "crypto/blake3.hpp"

// Core components
#include "core/node/node.hpp"
#include "core/node/node_identity.hpp"
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

namespace {

template <typename T>
T get_config_value(
    const cashew::utils::Config& config,
    const std::string& flat_key,
    const std::vector<std::string>& nested_path,
    const T& default_value
) {
    if (auto flat = config.get<T>(flat_key)) {
        return *flat;
    }

    const auto& root = config.data();
    const cashew::utils::json* current = &root;
    for (const auto& segment : nested_path) {
        if (!current->is_object() || !current->contains(segment)) {
            return default_value;
        }
        current = &(*current)[segment];
    }

    try {
        return current->get<T>();
    } catch (...) {
        return default_value;
    }
}

void print_usage() {
    std::cout << "Cashew CLI\n\n";
    std::cout << "Usage:\n";
    std::cout << "  cashew node [config_path]            Start node and gateway\n";
    std::cout << "  cashew content add <file> [--mime M] Add content to local storage\n";
    std::cout << "  cashew content list                  List locally stored content hashes\n";
    std::cout << "  cashew share <hash> [--gateway URL] [--config FILE] Print share links for content hash\n";
    std::cout << "  cashew help                          Show this help\n\n";
    std::cout << "Backward compatibility:\n";
    std::cout << "  cashew [config_path]                 Same as: cashew node [config_path]\n";
}

cashew::utils::Config load_config(const std::string& config_path) {
    cashew::utils::Config config;
    if (std::filesystem::exists(config_path)) {
        return cashew::utils::Config::load_from_file(config_path);
    }

    config.set("log_level", "info");
    config.set("log_to_file", false);
    config.set("identity_file", "cashew_identity.dat");
    config.set("identity_password", "");
    config.set("data_dir", "./data");
    config.set("http_port", 8080);
    config.set("web_root", "./test-site");
    return config;
}

std::optional<cashew::bytes> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return std::nullopt;
    }

    const auto size = file.tellg();
    if (size < 0) {
        return std::nullopt;
    }

    file.seekg(0, std::ios::beg);
    cashew::bytes data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) {
        return std::nullopt;
    }
    return data;
}

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string detect_mime_from_path(const std::filesystem::path& path) {
    const std::string ext = to_lower(path.extension().string());
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
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".webm") return "video/webm";
    return "application/octet-stream";
}

int run_content_add(const std::string& config_path, int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: cashew content add <file> [--mime M]\n";
        return 1;
    }

    const std::filesystem::path content_path = argv[3];
    if (!std::filesystem::exists(content_path)) {
        std::cerr << "File not found: " << content_path.string() << "\n";
        return 1;
    }

    std::string mime_override;
    for (int i = 4; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--mime") {
            mime_override = argv[i + 1];
            ++i;
        }
    }

    const auto file_data = read_file_bytes(content_path);
    if (!file_data) {
        std::cerr << "Failed to read file: " << content_path.string() << "\n";
        return 1;
    }

    auto hash = cashew::crypto::Blake3::hash(*file_data);
    cashew::ContentHash content_hash(hash);

    auto config = load_config(config_path);
    const auto data_dir = get_config_value<std::string>(
        config, "data_dir", {"storage", "data_dir"}, "./data"
    );
    cashew::storage::Storage storage(std::filesystem::path(data_dir) / "storage");

    if (!storage.put_content(content_hash, *file_data)) {
        std::cerr << "Failed to store content\n";
        return 1;
    }

    const std::string mime = mime_override.empty() ? detect_mime_from_path(content_path) : mime_override;
    const std::string hash_str = content_hash.to_string();
    const std::string filename = content_path.filename().string();

    storage.put_metadata("mime_" + hash_str, cashew::bytes(mime.begin(), mime.end()));
    storage.put_metadata("name_" + hash_str, cashew::bytes(filename.begin(), filename.end()));

    const auto gateway_port = get_config_value<uint16_t>(
        config, "http_port", {"gateway", "http_port"}, 8080
    );
    std::cout << "Stored content\n";
    std::cout << "  Hash: " << hash_str << "\n";
    std::cout << "  File: " << filename << "\n";
    std::cout << "  MIME: " << mime << "\n";
    std::cout << "  API:  http://localhost:" << gateway_port << "/api/thing/" << hash_str << "\n";
    return 0;
}

int run_content_list(const std::string& config_path) {
    auto config = load_config(config_path);
    const auto data_dir = get_config_value<std::string>(
        config, "data_dir", {"storage", "data_dir"}, "./data"
    );
    cashew::storage::Storage storage(std::filesystem::path(data_dir) / "storage");

    const auto items = storage.list_content();
    std::cout << "Stored content: " << items.size() << " item(s)\n";
    for (const auto& item : items) {
        std::cout << "  " << item.to_string() << "\n";
    }
    return 0;
}

int run_share(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: cashew share <hash> [--gateway URL] [--config FILE]\n";
        return 1;
    }

    std::string hash = argv[2];
    std::string config_path = "cashew.conf";
    for (int i = 3; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--config") {
            config_path = argv[i + 1];
            ++i;
        }
    }

    const auto config = load_config(config_path);
    std::string gateway;
    if (config.has("public_gateway") || (config.data().contains("gateway") && config.data()["gateway"].contains("public_gateway"))) {
        gateway = get_config_value<std::string>(
            config, "public_gateway", {"gateway", "public_gateway"}, "http://localhost:8080"
        );
    } else {
        const auto port = get_config_value<uint16_t>(
            config, "http_port", {"gateway", "http_port"}, 8080
        );
        gateway = "http://localhost:" + std::to_string(port);
    }

    for (int i = 3; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--gateway") {
            gateway = argv[i + 1];
            ++i;
        }
    }

    if (hash.size() != 64) {
        std::cerr << "Hash must be 64 hex chars\n";
        return 1;
    }

    std::cout << "Share this content\n";
    std::cout << "  Hash: " << hash << "\n";
    std::cout << "  API:  " << gateway << "/api/thing/" << hash << "\n";
    std::cout << "  Browser: " << gateway << "/api/thing/" << hash << "\n";
    std::cout << "  Test: curl " << gateway << "/api/thing/" << hash << "\n";
    return 0;
}

int run_node(const std::string& config_path) {
    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto config = load_config(config_path);

    // Initialize logging
    auto log_level = get_config_value<std::string>(
        config, "log_level", {"node", "log_level"}, "info"
    );
    auto log_to_file = get_config_value<bool>(
        config, "log_to_file", {"node", "log_to_file"}, false
    );
    cashew::utils::Logger::init(log_level, log_to_file);

    CASHEW_LOG_INFO("=================================================");
    CASHEW_LOG_INFO("    Cashew Network Node v{}.{}.{}",
        CASHEW_VERSION_MAJOR,
        CASHEW_VERSION_MINOR,
        CASHEW_VERSION_PATCH
    );
    CASHEW_LOG_INFO("   Freedom over profit. Privacy over surveillance.");
    CASHEW_LOG_INFO("=================================================");

    // Real identity boot path (persistent)
    const std::filesystem::path identity_path =
        get_config_value<std::string>(config, "identity_file", {"node", "identity_file"}, "cashew_identity.dat");
    const std::string identity_password = get_config_value<std::string>(
        config, "identity_password", {"node", "identity_password"}, ""
    );

    cashew::core::NodeIdentity identity =
        std::filesystem::exists(identity_path)
            ? cashew::core::NodeIdentity::load(identity_path, identity_password)
            : cashew::core::NodeIdentity::generate();

    if (!std::filesystem::exists(identity_path)) {
        identity.save(identity_path, identity_password);
        CASHEW_LOG_INFO("Created node identity: {}", identity_path.string());
    } else {
        CASHEW_LOG_INFO("Loaded node identity: {}", identity_path.string());
    }

    const cashew::NodeID node_id = identity.id();
    CASHEW_LOG_INFO("Node ID: {}", node_id.to_string());

    // ========================================================================
    // INITIALIZE CORE COMPONENTS
    // ========================================================================

    // 1. Storage Backend
    CASHEW_LOG_INFO("Initializing storage backend...");
    auto data_dir = get_config_value<std::string>(
        config, "data_dir", {"storage", "data_dir"}, "./data"
    );
    auto storage = std::make_shared<cashew::storage::Storage>(
        std::filesystem::path(data_dir) / "storage"
    );
    CASHEW_LOG_INFO("Storage initialized: {} items, {} bytes",
        storage->item_count(), storage->total_size());

    // 2. Ledger
    CASHEW_LOG_INFO("Initializing ledger...");
    auto ledger = std::make_shared<cashew::ledger::Ledger>(node_id);
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
    gateway_config.http_port = get_config_value<uint16_t>(
        config, "http_port", {"gateway", "http_port"}, 8080
    );
    gateway_config.web_root = get_config_value<std::string>(
        config, "web_root", {"gateway", "web_root"}, "./test-site"
    );
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
    CASHEW_LOG_INFO("  Node ID:    {}", node_id.to_string());
    CASHEW_LOG_INFO("  Storage:    {} items", storage->item_count());
    CASHEW_LOG_INFO("  Networks:   {}", network_registry->total_network_count());
    CASHEW_LOG_INFO("  Ledger:     {} events", ledger->event_count());
    CASHEW_LOG_INFO("");
    CASHEW_LOG_INFO("Press Ctrl+C to shutdown");
    CASHEW_LOG_INFO("");

    while (!g_shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    CASHEW_LOG_INFO("Shutting down...");
    CASHEW_LOG_INFO("Stopping gateway server...");
    gateway->stop();

    CASHEW_LOG_INFO("Stopping WebSocket handler...");
    websocket_handler->stop();

    CASHEW_LOG_INFO("Saving network state...");
    std::filesystem::create_directories(networks_dir);
    network_registry->save_to_disk(networks_dir);

    CASHEW_LOG_INFO("");
    CASHEW_LOG_INFO("Node stopped. Goodbye! ");
    CASHEW_LOG_INFO("");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string default_config = "cashew.conf";

        if (argc <= 1) {
            return run_node(default_config);
        }

        const std::string command = argv[1];

        if (command == "help" || command == "--help" || command == "-h") {
            print_usage();
            return 0;
        }

        if (command == "node") {
            const std::string config_path = (argc >= 3) ? argv[2] : default_config;
            return run_node(config_path);
        }

        if (command == "content") {
            if (argc < 3) {
                std::cerr << "Usage: cashew content <add|list> ...\n";
                return 1;
            }

            const std::string subcommand = argv[2];
            if (subcommand == "add") {
                return run_content_add(default_config, argc, argv);
            }
            if (subcommand == "list") {
                return run_content_list(default_config);
            }

            std::cerr << "Unknown content subcommand: " << subcommand << "\n";
            return 1;
        }

        if (command == "share") {
            return run_share(argc, argv);
        }

        // Backward compatibility: treat argument as config path
        return run_node(command);
        
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}
