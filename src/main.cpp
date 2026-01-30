#include <iostream>
#include <memory>
#include "core/node/node.hpp"
#include "utils/logger.hpp"
#include "utils/config.hpp"

int main(int argc, char** argv) {
    cashew::utils::Logger::init();
    
    CASHEW_LOG_INFO("Cashew Node starting...");
    CASHEW_LOG_INFO("Version: {}.{}.{}", 
        CASHEW_VERSION_MAJOR, 
        CASHEW_VERSION_MINOR, 
        CASHEW_VERSION_PATCH
    );
    
    try {
        // Load configuration
        auto config = cashew::utils::Config::load_from_file("cashew.conf");
        
        // Create and initialize node
        auto node = std::make_unique<cashew::core::Node>(config);
        node->initialize();
        
        CASHEW_LOG_INFO("Node ID: {}", node->get_node_id().to_string());
        CASHEW_LOG_INFO("Node started successfully");
        
        // Run node (blocks until shutdown)
        node->run();
        
        CASHEW_LOG_INFO("Node shutting down...");
        node->shutdown();
        
    } catch (const std::exception& e) {
        CASHEW_LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }
    
    CASHEW_LOG_INFO("Cashew Node stopped");
    return 0;
}
