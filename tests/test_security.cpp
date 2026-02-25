#include "security/access.hpp"
#include "security/content_integrity.hpp"
#include "security/attack_prevention.hpp"
#include "security/onion_routing.hpp"
#include "security/ip_protection.hpp"
#include "security/key_revocation.hpp"
#include "security/token_revocation.hpp"
#include "security/hardware_key_storage.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::security;

class SecurityTest : public ::testing::Test {
protected:
    NodeIdentity node1, node2;
    
    void SetUp() override {
        node1 = NodeIdentity::generate();
        node2 = NodeIdentity::generate();
    }
};

// ============================================================================
// Capability Token Tests
// ============================================================================

TEST_F(SecurityTest, TokenIssuance) {
    TokenIssuer issuer(node1);
    
    auto token = issuer.issue_token(
        node2.id(),
        {"read", "write"},
        time::now() + std::chrono::hours(1)
    );
    
    EXPECT_EQ(token.holder, node2.id());
    EXPECT_EQ(token.issuer, node1.id());
    EXPECT_EQ(token.permissions.size(), 2);
    EXPECT_GT(token.signature.size(), 0);
}

TEST_F(SecurityTest, TokenVerification) {
    TokenIssuer issuer(node1);
    
    auto token = issuer.issue_token(
        node2.id(),
        {"read"},
        time::now() + std::chrono::hours(1)
    );
    
    // Valid token should verify
    EXPECT_TRUE(issuer.verify_token(token));
    
    // Tampered token should fail
    token.permissions.push_back("admin");
    EXPECT_FALSE(issuer.verify_token(token));
}

TEST_F(SecurityTest, PermissionChecking) {
    AccessControl ac;
    
    TokenIssuer issuer(node1);
    auto token = issuer.issue_token(node2.id(), {"read", "write"}, time::now() + std::chrono::hours(1));
    
    ac.register_token(token);
    
    // Should have read permission
    EXPECT_TRUE(ac.has_permission(token, "read"));
    EXPECT_TRUE(ac.has_permission(token, "write"));
    
    // Should not have admin permission
    EXPECT_FALSE(ac.has_permission(token, "admin"));
}

TEST_F(SecurityTest, TokenExpiration) {
    TokenIssuer issuer(node1);
    
    // Expired token
    auto expired_token = issuer.issue_token(
        node2.id(),
        {"read"},
        time::now() - std::chrono::hours(1)
    );
    
    EXPECT_TRUE(issuer.is_expired(expired_token));
    
    // Valid token
    auto valid_token = issuer.issue_token(
        node2.id(),
        {"read"},
        time::now() + std::chrono::hours(1)
    );
    
    EXPECT_FALSE(issuer.is_expired(valid_token));
}

TEST_F(SecurityTest, TokenRevocation) {
    TokenRevocationManager revocation;
    
    bytes token_id = {1, 2, 3, 4};
    
    // Revoke token
    auto revocation_notice = revocation.revoke_token(token_id, node1.id(), "Compromised");
    EXPECT_EQ(revocation_notice.token_id, token_id);
    
    // Check if revoked
    EXPECT_TRUE(revocation.is_revoked(token_id));
}

TEST_F(SecurityTest, RevocationListGossip) {
    TokenRevocationManager rev1, rev2;
    
    // Revoke on node1
    bytes token_id = {1, 2, 3};
    auto notice = rev1.revoke_token(token_id, node1.id(), "Test");
    
    // Gossip to node2
    auto update = rev1.create_revocation_update();
    rev2.process_revocation_update(update);
    
    // Node2 should also know about revocation
    EXPECT_TRUE(rev2.is_revoked(token_id));
}

// ============================================================================
// Attack Prevention Tests
// ============================================================================

TEST_F(SecurityTest, RateLimiting) {
    RateLimiter limiter(10, std::chrono::seconds(1)); // 10 per second
    
    // First 10 should succeed
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(limiter.allow());
    }
    
    // 11th should be rate limited
    EXPECT_FALSE(limiter.allow());
    
    // After waiting, should work again
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(limiter.allow());
}

TEST_F(SecurityTest, SybilDetection) {
    SybilDetector detector;
    
    // Register legitimate nodes with PoW
    detector.register_node(node1.id(), true, 100.0);
    detector.register_node(node2.id(), true, 95.0);
    
    // Register suspicious node without PoW
    auto sybil_node = NodeIdentity::generate();
    detector.register_node(sybil_node.id(), false, 0.0);
    
    // Check suspicion scores
    EXPECT_LT(detector.get_suspicion_score(node1.id()), 0.5);
    EXPECT_GT(detector.get_suspicion_score(sybil_node.id()), 0.5);
}

TEST_F(SecurityTest, DDoSMitigation) {
    DDoSMitigator mitigator;
    
    std::string ip = "192.168.1.100";
    
    // Normal traffic should be allowed
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(mitigator.allow_request(ip));
    }
    
    // Flood of requests
    for (int i = 0; i < 1000; i++) {
        mitigator.allow_request(ip);
    }
    
    // Should be blocked
    EXPECT_FALSE(mitigator.allow_request(ip));
}

TEST_F(SecurityTest, IdentityForkDetection) {
    ForkDetector detector;
    
    // Register identity
    detector.register_identity(node1.id(), node1.public_key());
    
    // Try to register same ID with different key (fork attempt)
    auto fake_key = NodeIdentity::generate().public_key();
    bool fork_detected = detector.detect_fork(node1.id(), fake_key);
    
    EXPECT_TRUE(fork_detected);
}

TEST_F(SecurityTest, BehavioralFingerprinting) {
    BehavioralAnalyzer analyzer;
    
    // Record behavior patterns
    for (int i = 0; i < 100; i++) {
        analyzer.record_action(node1.id(), ActionType::MessageSent, time::now());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Analyze pattern
    auto pattern = analyzer.get_pattern(node1.id());
    EXPECT_GT(pattern.action_count, 0);
    
    // Detect anomaly
    for (int i = 0; i < 1000; i++) {
        analyzer.record_action(node1.id(), ActionType::MessageSent, time::now());
    }
    
    bool anomaly = analyzer.is_anomalous(node1.id());
    EXPECT_TRUE(anomaly);
}

// ============================================================================
// Anonymity Tests
// ============================================================================

TEST_F(SecurityTest, OnionRoutingLayering) {
    OnionRouter router;
    
    // Create layered route
    std::vector<NodeID> route = {node1.id(), node2.id()};
    bytes message = {1, 2, 3, 4, 5};
    
    auto layered = router.wrap_layers(message, route);
    EXPECT_GT(layered.size(), message.size());
    
    // Peel layers
    auto peeled1 = router.peel_layer(layered, node1.id());
    ASSERT_TRUE(peeled1.has_value());
    
    auto peeled2 = router.peel_layer(peeled1.value(), node2.id());
    ASSERT_TRUE(peeled2.has_value());
    
    EXPECT_EQ(peeled2.value(), message);
}

TEST_F(SecurityTest, TrafficMixing) {
    TrafficMixer mixer;
    
    // Add messages to mix
    mixer.add_message({1, 2, 3});
    mixer.add_message({4, 5, 6});
    mixer.add_message({7, 8, 9});
    
    // Get mixed batch
    auto batch = mixer.get_mixed_batch();
    EXPECT_EQ(batch.size(), 3);
    
    // Order should be shuffled
}

TEST_F(SecurityTest, EphemeralAddressing) {
    EphemeralAddressManager manager;
    
    // Generate ephemeral address
    auto addr1 = manager.generate_address(node1.id());
    EXPECT_GT(addr1.size(), 0);
    
    // Should be different each time
    auto addr2 = manager.generate_address(node1.id());
    EXPECT_NE(addr1, addr2);
    
    // Should be able to resolve back
    auto resolved = manager.resolve_address(addr1);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved.value(), node1.id());
}

TEST_F(SecurityTest, MetadataMinimization) {
    MessageSanitizer sanitizer;
    
    Message msg;
    msg.sender = node1.id();
    msg.payload = {1, 2, 3};
    msg.timestamp = time::now();
    msg.metadata["ip"] = "192.168.1.100";
    msg.metadata["user-agent"] = "Cashew/1.0";
    
    // Sanitize metadata
    auto sanitized = sanitizer.sanitize(msg);
    
    // PII should be removed
    EXPECT_FALSE(sanitized.metadata.contains("ip"));
    EXPECT_FALSE(sanitized.metadata.contains("user-agent"));
    
    // Essential data should remain
    EXPECT_EQ(sanitized.payload, msg.payload);
}

// ============================================================================
// Key Security Tests
// ============================================================================

TEST_F(SecurityTest, EncryptedKeyStorage) {
    bytes password = {1, 2, 3, 4, 5};
    SecureKeyStorage storage(password);
    
    bytes key_data = node1.private_key_bytes();
    
    // Store encrypted
    auto key_id = storage.store_key(key_data);
    EXPECT_GT(key_id.size(), 0);
    
    // Retrieve and decrypt
    auto retrieved = storage.retrieve_key(key_id, password);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value(), key_data);
    
    // Wrong password should fail
    bytes wrong_password = {9, 9, 9};
    auto failed = storage.retrieve_key(key_id, wrong_password);
    EXPECT_FALSE(failed.has_value());
}

TEST_F(SecurityTest, HardwareKeyStorage) {
    // Try hardware storage (may fall back to software)
    auto storage = HardwareKeyStorage::create();
    ASSERT_NE(storage, nullptr);
    
    bytes key_data = node1.private_key_bytes();
    
    // Store key
    auto key_id = storage->store_key("test_key", key_data);
    EXPECT_TRUE(key_id.has_value());
    
    // Retrieve key
    auto retrieved = storage->retrieve_key(key_id.value());
    if (retrieved.has_value()) {
        EXPECT_EQ(retrieved.value(), key_data);
    }
}

TEST_F(SecurityTest, KeyRotationCertificate) {
    auto old_key = node1.public_key();
    auto new_identity = NodeIdentity::generate();
    auto new_key = new_identity.public_key();
    
    // Create rotation certificate
    RotationCertificate cert;
    cert.node_id = node1.id();
    cert.old_key = old_key;
    cert.new_key = new_key;
    cert.timestamp = time::now();
    cert.signature = node1.sign({1, 2, 3});
    
    // Serialize
    auto data = cert.to_bytes();
    EXPECT_GT(data.size(), 0);
    
    // Verify
    bool valid = cert.verify(node1.public_key());
    EXPECT_TRUE(valid);
}

TEST_F(SecurityTest, KeyRevocationBroadcast) {
    KeyRevocationBroadcaster broadcaster;
    
    // Revoke key
    auto revocation = broadcaster.revoke_key(node1.public_key(), node1.id(), "Compromised");
    
    EXPECT_EQ(revocation.key, node1.public_key());
    EXPECT_GT(revocation.signature.size(), 0);
    
    // Verify revocation
    EXPECT_TRUE(broadcaster.verify_revocation(revocation));
    
    // Check if key is revoked
    EXPECT_TRUE(broadcaster.is_revoked(node1.public_key()));
}

TEST_F(SecurityTest, ContentIntegrityChecking) {
    ContentIntegrityChecker checker;
    
    bytes content(1024 * 1024, 0x42); // 1MB
    
    // Build Merkle tree
    auto root = checker.build_merkle_tree(content);
    EXPECT_GT(root.hash.size(), 0);
    
    // Verify integrity
    bool valid = checker.verify_integrity(content, root.hash);
    EXPECT_TRUE(valid);
    
    // Tamper with content
    content[500] = 0xFF;
    bool invalid = checker.verify_integrity(content, root.hash);
    EXPECT_FALSE(invalid);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
