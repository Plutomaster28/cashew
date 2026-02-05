#include "node.hpp"
#include "../../utils/logger.hpp"
#include <filesystem>
#include <thread>
#include <chrono>

namespace cashew::core {

class Node::Impl {
public:
    utils::Config config;
    std::unique_ptr<NodeIdentity> identity;
    bool running = false;
    
    explicit Impl(const utils::Config& cfg) : config(cfg) {}
};

Node::Node(const utils::Config& config)
    : impl_(std::make_unique<Impl>(config))
{
}

Node::~Node() = default;

void Node::initialize() {
    CASHEW_LOG_INFO("Initializing Cashew node...");
    
    // Get identity file path from config
    auto identity_path = impl_->config.get_or<std::string>("identity_file", "cashew_identity.dat");
    auto identity_password = impl_->config.get_or<std::string>("identity_password", "");
    
    // Load or generate identity
    if (std::filesystem::exists(identity_path)) {
        CASHEW_LOG_INFO("Loading existing identity from: {}", identity_path);
        impl_->identity = std::make_unique<NodeIdentity>(
            NodeIdentity::load(identity_path, identity_password)
        );
    } else {
        CASHEW_LOG_INFO("Generating new identity");
        impl_->identity = std::make_unique<NodeIdentity>(NodeIdentity::generate());
        impl_->identity->save(identity_path, identity_password);
    }
    
    CASHEW_LOG_INFO("Node initialized with ID: {}", impl_->identity->id().to_string());
}

void Node::run() {
    CASHEW_LOG_INFO("Starting node...");
    impl_->running = true;
    
    // TODO: Implement main event loop
    // For now, just wait for shutdown
    while (impl_->running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Node::shutdown() {
    CASHEW_LOG_INFO("Shutting down node...");
    impl_->running = false;
    
    // TODO: Cleanup resources
}

const NodeID& Node::get_node_id() const {
    if (!impl_->identity) {
        throw std::runtime_error("Node not initialized");
    }
    return impl_->identity->id();
}

} // namespace cashew::core
