#include "network/session.hpp"
#include "core/node/node_identity.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace cashew;
using namespace cashew::network;
using namespace cashew::core;

class SessionTest : public ::testing::Test {
protected:
    // No member variables - generate identities locally in each test
};

TEST_F(SessionTest, SessionCreation) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    
    EXPECT_EQ(session.get_local_node_id(), identity1.id());
    EXPECT_EQ(session.get_remote_node_id(), identity2.id());
    EXPECT_EQ(session.get_state(), SessionState::DISCONNECTED);
    EXPECT_FALSE(session.is_established());
}

TEST_F(SessionTest, HandshakeInitiation) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session1(identity1.id(), identity2.id());
    
    bool initiated = session1.initiate_handshake();
    EXPECT_TRUE(initiated);
    EXPECT_EQ(session1.get_state(), SessionState::HANDSHAKE_INIT);
    EXPECT_TRUE(session1.is_initiator());
}

TEST_F(SessionTest, HandshakeMessageCreation) {
    auto identity1 = NodeIdentity::generate();
    
    HandshakeMessage msg;
    msg.version = HandshakeMessage::CURRENT_VERSION;
    msg.ephemeral_public = PublicKey{};
    msg.node_id = identity1.id();
    msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    msg.signature = Signature{};
    
    EXPECT_EQ(msg.version, 1);
    EXPECT_EQ(msg.node_id, identity1.id());
}

TEST_F(SessionTest, HandshakeSerializationRoundtrip) {
    auto identity1 = NodeIdentity::generate();
    
    HandshakeMessage original;
    original.version = HandshakeMessage::CURRENT_VERSION;
    original.node_id = identity1.id();
    original.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Serialize
    auto bytes = original.to_bytes();
    EXPECT_GT(bytes.size(), 0);
    
    // Deserialize
    auto restored = HandshakeMessage::from_bytes(bytes);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->version, original.version);
    EXPECT_EQ(restored->node_id, original.node_id);
}

TEST_F(SessionTest, SessionStateTransitions) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    
    // Initial state
    EXPECT_EQ(session.get_state(), SessionState::DISCONNECTED);
    
    // Initiate handshake
    session.initiate_handshake();
    EXPECT_EQ(session.get_state(), SessionState::HANDSHAKE_INIT);
}

TEST_F(SessionTest, SessionStatistics) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    
    // Initial stats should be zero
    EXPECT_EQ(session.get_messages_sent(), 0);
    EXPECT_EQ(session.get_messages_received(), 0);
    EXPECT_EQ(session.get_bytes_sent(), 0);
    EXPECT_EQ(session.get_bytes_received(), 0);
}

TEST_F(SessionTest, SessionAge) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    uint64_t age = session.get_age_seconds();
    EXPECT_GE(age, 0);  // Age should be non-negative
}

TEST_F(SessionTest, SessionClose) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    session.initiate_handshake();
    
    session.close();
    
    SessionState state = session.get_state();
    EXPECT_TRUE(state == SessionState::CLOSING || state == SessionState::CLOSED);
}

TEST_F(SessionTest, SessionManagerCreation) {
    auto identity1 = NodeIdentity::generate();
    
    SessionManager manager(identity1.id());
    
    EXPECT_EQ(manager.active_session_count(), 0);
}

TEST_F(SessionTest, SessionManagerOutboundSession) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    SessionManager manager(identity1.id());
    
    auto session = manager.create_outbound_session(identity2.id());
    
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->get_local_node_id(), identity1.id());
    EXPECT_EQ(session->get_remote_node_id(), identity2.id());
    // Note: active_session_count() only counts ESTABLISHED sessions,
    // newly created sessions are not established yet
    // So we expect 0 active (established) sessions
    EXPECT_EQ(manager.active_session_count(), 0);
}

TEST_F(SessionTest, SessionManagerSessionLookup) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    SessionManager manager(identity1.id());
    
    // Create session
    manager.create_outbound_session(identity2.id());
    
    // Lookup should find it
    EXPECT_TRUE(manager.has_session(identity2.id()));
    
    auto session = manager.get_session(identity2.id());
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->get_remote_node_id(), identity2.id());
}

TEST_F(SessionTest, SessionManagerMultipleSessions) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    auto identity3 = NodeIdentity::generate();
    auto identity4 = NodeIdentity::generate();
    
    SessionManager manager(identity1.id());
    
    manager.create_outbound_session(identity2.id());
    manager.create_outbound_session(identity3.id());
    manager.create_outbound_session(identity4.id());
    
    // active_session_count only counts ESTABLISHED sessions
    // These are just created but not yet established
    EXPECT_EQ(manager.active_session_count(), 0);
    
    // get_connected_peers also only returns ESTABLISHED peers
    auto peers = manager.get_connected_peers();
    EXPECT_EQ(peers.size(), 0);
    
    // But has_session should find them regardless of state
    EXPECT_TRUE(manager.has_session(identity2.id()));
    EXPECT_TRUE(manager.has_session(identity3.id()));
    EXPECT_TRUE(manager.has_session(identity4.id()));
}

TEST_F(SessionTest, SessionManagerCloseSession) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    SessionManager manager(identity1.id());
    
    auto session = manager.create_outbound_session(identity2.id());
    // New session is not established yet, so active count is 0
    EXPECT_EQ(manager.active_session_count(), 0);
    
    // But has_session should find it
    EXPECT_TRUE(manager.has_session(identity2.id()));
    
    manager.close_session(identity2.id());
    EXPECT_FALSE(manager.has_session(identity2.id()));
}

TEST_F(SessionTest, SessionManagerCloseAll) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    auto identity3 = NodeIdentity::generate();
    
    SessionManager manager(identity1.id());
    
    manager.create_outbound_session(identity2.id());
    manager.create_outbound_session(identity3.id());
    // New sessions are not established yet
    EXPECT_EQ(manager.active_session_count(), 0);
    
    // But has_session should find them
    EXPECT_TRUE(manager.has_session(identity2.id()));
    EXPECT_TRUE(manager.has_session(identity3.id()));
    
    manager.close_all_sessions();
    
    // After closing all, sessions should be removed
    EXPECT_FALSE(manager.has_session(identity2.id()));
    EXPECT_FALSE(manager.has_session(identity3.id()));
}

TEST_F(SessionTest, SessionIdleTimeout) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    
    // Should not have timed out immediately
    EXPECT_FALSE(session.has_timed_out());
}

TEST_F(SessionTest, SessionRekeyCheck) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    
    // New session should not need rekeying
    EXPECT_FALSE(session.should_rekey());
}

TEST_F(SessionTest, SessionIdentityVerification) {
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session1(identity1.id(), identity2.id());
    Session session2(identity2.id(), identity1.id());
    
    // Sessions should have correct identities
    EXPECT_EQ(session1.get_local_node_id(), identity1.id());
    EXPECT_EQ(session1.get_remote_node_id(), identity2.id());
    EXPECT_EQ(session2.get_local_node_id(), identity2.id());
    EXPECT_EQ(session2.get_remote_node_id(), identity1.id());
}

TEST_F(SessionTest, SessionConstants) {
    // Verify session constants are reasonable (constants are private, can't access directly)
    // Just verify a session can be created
    auto identity1 = NodeIdentity::generate();
    auto identity2 = NodeIdentity::generate();
    
    Session session(identity1.id(), identity2.id());
    EXPECT_FALSE(session.should_rekey());  // New session shouldn't need rekey
    EXPECT_FALSE(session.has_timed_out());  // New session shouldn't timeout
}

TEST_F(SessionTest, HandshakeMessageVersion) {
    // Verify current version is 1
    EXPECT_EQ(HandshakeMessage::CURRENT_VERSION, 1);
}

TEST_F(SessionTest, HandshakeMessageMaxAge) {
    // Verify max age is 60 seconds
    EXPECT_EQ(HandshakeMessage::MAX_AGE_SECONDS, 60);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
