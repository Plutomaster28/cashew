#include "network/network.hpp"
#include "network/session.hpp"
#include "network/connection.hpp"
#include "network/peer.hpp"
#include "network/gossip.hpp"
#include "network/router.hpp"
#include "core/ledger/ledger.hpp"
#include "core/reputation/reputation.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::network;
using namespace cashew::core;

class NetworkTest : public ::testing::Test {
protected:
    NodeIdentity node1, node2, node3;
    
    void SetUp() override {
        node1 = NodeIdentity::generate();
        node2 = NodeIdentity::generate();
        node3 = NodeIdentity::generate();
    }
};

// ============================================================================
// Session Tests - Encrypted communication
// ============================================================================

TEST_F(NetworkTest, SessionCreation) {
    Session session1(node1);
    Session session2(node2);
    
    EXPECT_EQ(session1.local_id(), node1.id());
    EXPECT_EQ(session2.local_id(), node2.id());
}

TEST_F(NetworkTest, SessionHandshake) {
    Session session1(node1);
    Session session2(node2);
    
    // Perform handshake
    auto handshake1 = session1.initiate_handshake(node2.id());
    ASSERT_TRUE(handshake1.has_value());
    
    auto handshake2 = session2.process_handshake(handshake1.value());
    ASSERT_TRUE(handshake2.has_value());
    
    session1.complete_handshake(handshake2.value());
    
    // Both sessions should be established
    EXPECT_TRUE(session1.is_established());
    EXPECT_TRUE(session2.is_established());
}

TEST_F(NetworkTest, EncryptedMessaging) {
    Session session1(node1);
    Session session2(node2);
    
    // Establish session
    auto hs1 = session1.initiate_handshake(node2.id());
    auto hs2 = session2.process_handshake(hs1.value());
    session1.complete_handshake(hs2.value());
    
    // Send encrypted message
    bytes plaintext = {1, 2, 3, 4, 5};
    auto encrypted = session1.encrypt(plaintext);
    ASSERT_TRUE(encrypted.has_value());
    
    // Decrypt message
    auto decrypted = session2.decrypt(encrypted.value());
    ASSERT_TRUE(decrypted.has_value());
    
    EXPECT_EQ(plaintext, decrypted.value());
}

TEST_F(NetworkTest, SessionTimeout) {
    Session session(node1);
    session.initiate_handshake(node2.id());
    
    // Check timeout
    EXPECT_FALSE(session.is_expired(std::chrono::minutes(29)));
    EXPECT_TRUE(session.is_expired(std::chrono::minutes(31)));
}

TEST_F(NetworkTest, SessionRekeying) {
    Session session1(node1);
    Session session2(node2);
    
    // Establish session
    auto hs1 = session1.initiate_handshake(node2.id());
    auto hs2 = session2.process_handshake(hs1.value());
    session1.complete_handshake(hs2.value());
    
    // Send lots of data to trigger rekey
    for (int i = 0; i < 100; i++) {
        bytes data(1024 * 1024, 0x42); // 1MB each
        auto encrypted = session1.encrypt(data);
        if (encrypted.has_value()) {
            session2.decrypt(encrypted.value());
        }
    }
    
    // Session should still work after rekey
    bytes test_data = {1, 2, 3};
    auto encrypted = session1.encrypt(test_data);
    ASSERT_TRUE(encrypted.has_value());
    
    auto decrypted = session2.decrypt(encrypted.value());
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(test_data, decrypted.value());
}

// ============================================================================
// Peer Discovery Tests
// ============================================================================

TEST_F(NetworkTest, PeerAnnouncement) {
    PeerManager manager;
    
    // Add peer
    SocketAddress addr{"192.168.1.100", 8080};
    manager.add_peer(node1.id(), addr);
    
    // Retrieve peer
    auto peer = manager.get_peer(node1.id());
    ASSERT_TRUE(peer.has_value());
    EXPECT_EQ(peer->node_id, node1.id());
    EXPECT_EQ(peer->address.host, "192.168.1.100");
}

TEST_F(NetworkTest, PeerDatabase) {
    PeerManager manager;
    
    // Add multiple peers
    manager.add_peer(node1.id(), {"192.168.1.100", 8080});
    manager.add_peer(node2.id(), {"192.168.1.101", 8080});
    manager.add_peer(node3.id(), {"192.168.1.102", 8080});
    
    auto peers = manager.get_all_peers();
    EXPECT_EQ(peers.size(), 3);
}

TEST_F(NetworkTest, RandomPeerSelection) {
    PeerManager manager;
    
    // Add peers
    for (int i = 0; i < 10; i++) {
        auto node = NodeIdentity::generate();
        manager.add_peer(node.id(), {"192.168.1." + std::to_string(100 + i), 8080});
    }
    
    // Select random peers
    auto selected = manager.select_random_peers(3);
    EXPECT_EQ(selected.size(), 3);
    
    // Should be different
    EXPECT_NE(selected[0].node_id, selected[1].node_id);
    EXPECT_NE(selected[1].node_id, selected[2].node_id);
}

// ============================================================================
// Gossip Protocol Tests
// ============================================================================

TEST_F(NetworkTest, GossipMessageCreation) {
    GossipProtocol gossip(node1.id());
    
    bytes payload = {1, 2, 3, 4, 5};
    auto message = gossip.create_message(GossipMessageType::Announcement, payload);
    
    EXPECT_EQ(message.sender, node1.id());
    EXPECT_EQ(message.type, GossipMessageType::Announcement);
    EXPECT_EQ(message.payload, payload);
    EXPECT_GT(message.signature.size(), 0);
}

TEST_F(NetworkTest, GossipMessageVerification) {
    GossipProtocol gossip(node1.id());
    
    bytes payload = {1, 2, 3};
    auto message = gossip.create_message(GossipMessageType::Announcement, payload);
    
    // Should verify correctly
    EXPECT_TRUE(gossip.verify_message(message));
    
    // Tampered message should fail
    message.payload.push_back(99);
    EXPECT_FALSE(gossip.verify_message(message));
}

TEST_F(NetworkTest, GossipDeduplication) {
    GossipProtocol gossip(node1.id());
    
    bytes payload = {1, 2, 3};
    auto message = gossip.create_message(GossipMessageType::Announcement, payload);
    
    // First time should be new
    EXPECT_TRUE(gossip.is_new_message(message));
    
    // Mark as seen
    gossip.mark_seen(message);
    
    // Second time should be duplicate
    EXPECT_FALSE(gossip.is_new_message(message));
}

TEST_F(NetworkTest, GossipPropagation) {
    GossipProtocol gossip1(node1.id());
    GossipProtocol gossip2(node2.id());
    GossipProtocol gossip3(node3.id());
    
    // Create message at node1
    bytes payload = {1, 2, 3};
    auto message = gossip1.create_message(GossipMessageType::Announcement, payload);
    
    // Propagate to node2
    auto peers_for_2 = gossip2.select_propagation_targets(message, {node3.id()});
    EXPECT_LE(peers_for_2.size(), 3); // Fanout = 3
    
    gossip2.mark_seen(message);
    
    // Propagate to node3
    gossip3.mark_seen(message);
    
    // All should have seen the message
    EXPECT_FALSE(gossip1.is_new_message(message));
    EXPECT_FALSE(gossip2.is_new_message(message));
    EXPECT_FALSE(gossip3.is_new_message(message));
}

// ============================================================================
// Routing Tests
// ============================================================================

TEST_F(NetworkTest, RoutingTableConstruction) {
    RoutingTable table(node1.id());
    
    // Add routes
    table.add_route(node2.id(), {node2.id()});
    table.add_route(node3.id(), {node2.id(), node3.id()});
    
    // Find routes
    auto route_to_2 = table.find_route(node2.id());
    ASSERT_TRUE(route_to_2.has_value());
    EXPECT_EQ(route_to_2->size(), 1);
    
    auto route_to_3 = table.find_route(node3.id());
    ASSERT_TRUE(route_to_3.has_value());
    EXPECT_EQ(route_to_3->size(), 2);
}

TEST_F(NetworkTest, ContentAddressedRouting) {
    Router router(node1.id());
    
    Hash256 content_hash;
    std::fill(content_hash.begin(), content_hash.end(), 0x42);
    
    // Should be able to route to content
    auto route = router.find_route_to_content(content_hash);
    // Route may or may not exist depending on network state
}

TEST_F(NetworkTest, HopLimitEnforcement) {
    Router router(node1.id());
    
    // Create a route with max hops
    std::vector<NodeID> long_route;
    for (int i = 0; i < 10; i++) {
        long_route.push_back(NodeIdentity::generate().id());
    }
    
    // Should reject routes exceeding hop limit
    EXPECT_FALSE(router.is_route_valid(long_route, 8));
    
    // Should accept routes within limit
    std::vector<NodeID> short_route{node2.id(), node3.id()};
    EXPECT_TRUE(router.is_route_valid(short_route, 8));
}

TEST_F(NetworkTest, RouteDiscovery) {
    Router router(node1.id());
    
    // Request route discovery
    auto request = router.initiate_route_discovery(node2.id());
    EXPECT_EQ(request.target, node2.id());
    EXPECT_EQ(request.initiator, node1.id());
    EXPECT_GT(request.request_id.size(), 0);
}

// ============================================================================
// Connection Tests
// ============================================================================

TEST_F(NetworkTest, TCPConnection) {
    TCPConnection conn;
    
    EXPECT_EQ(conn.state(), ConnectionState::DISCONNECTED);
    
    // Note: Actual connection test would require a server
    // This is a structural test
}

TEST_F(NetworkTest, ConnectionPooling) {
    ConnectionPool pool;
    
    SocketAddress addr1{"192.168.1.100", 8080};
    SocketAddress addr2{"192.168.1.101", 8080};
    
    // Get connections (will create if not exist)
    auto conn1 = pool.get_connection(addr1);
    auto conn2 = pool.get_connection(addr2);
    
    EXPECT_NE(conn1, nullptr);
    EXPECT_NE(conn2, nullptr);
    
    // Getting same address should reuse connection
    auto conn1_again = pool.get_connection(addr1);
    EXPECT_EQ(conn1, conn1_again);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
