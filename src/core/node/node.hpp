#pragma once

#include "node_identity.hpp"
#include "../../utils/config.hpp"
#include <memory>

namespace cashew::core {

/**
 * Cashew Node - main participant in the network
 */
class Node {
public:
    explicit Node(const utils::Config& config);
    ~Node();
    
    /**
     * Initialize the node (load/generate identity, setup storage, etc.)
     */
    void initialize();
    
    /**
     * Run the node (blocks until shutdown)
     */
    void run();
    
    /**
     * Shutdown the node gracefully
     */
    void shutdown();
    
    /**
     * Get the node's ID
     */
    const NodeID& get_node_id() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cashew::core
