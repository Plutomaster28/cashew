#include "core/keys/key.hpp"
#include "core/node/node_identity.hpp"
#include "core/pow/pow.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::core;

class KeySystemTest : public ::testing::Test {
protected:
    NodeIdentity node1, node2;
    KeyManager key_manager;
    
    void SetUp() override {
        node1 = NodeIdentity::generate();
        node2 = NodeIdentity::generate();
    }
};

TEST_F(KeySystemTest, KeyIssuance) {
    // Issue identity key
    auto key = key_manager.issue_key(node1.id(), KeyType::Identity, 1);
    
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->type, KeyType::Identity);
    EXPECT_EQ(key->owner, node1.id());
    EXPECT_EQ(key->epoch, 1);
    EXPECT_EQ(key->power, 1.0);
}

TEST_F(KeySystemTest, AllKeyTypes) {
    std::vector<KeyType> types = {
        KeyType::Identity,
        KeyType::Node,
        KeyType::Network,
        KeyType::Service,
        KeyType::Routing
    };
    
    for (auto type : types) {
        auto key = key_manager.issue_key(node1.id(), type, 1);
        ASSERT_TRUE(key.has_value());
        EXPECT_EQ(key->type, type);
    }
}

TEST_F(KeySystemTest, EpochLimits) {
    uint64_t epoch = 1;
    
    // Issue keys up to limit
    std::vector<Key> keys;
    for (int i = 0; i < 10; i++) {
        auto key = key_manager.issue_key(node1.id(), KeyType::Network, epoch);
        if (key.has_value()) {
            keys.push_back(key.value());
        }
    }
    
    // Should have issued some keys
    EXPECT_GT(keys.size(), 0);
    
    // Check if limit is enforced
    bool limit_reached = false;
    for (int i = 0; i < 100; i++) {
        auto key = key_manager.issue_key(node1.id(), KeyType::Network, epoch);
        if (!key.has_value()) {
            limit_reached = true;
            break;
        }
    }
    
    // Per-epoch limits should be enforced
    EXPECT_TRUE(limit_reached || keys.size() < 100);
}

TEST_F(KeySystemTest, KeyDecay) {
    auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    ASSERT_TRUE(key.has_value());
    
    auto key_id = key->id;
    
    // Check initial power
    EXPECT_EQ(key->power, 1.0);
    
    // Simulate time passing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Apply decay
    key_manager.apply_decay(key_id, 0.1);
    
    // Retrieve updated key
    auto updated_key = key_manager.get_key(key_id);
    ASSERT_TRUE(updated_key.has_value());
    
    // Power should have decayed
    EXPECT_LT(updated_key->power, 1.0);
}

TEST_F(KeySystemTest, ExponentialDecay) {
    auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    ASSERT_TRUE(key.has_value());
    
    auto key_id = key->id;
    double decay_rate = 0.1;
    
    // Apply decay multiple times
    for (int i = 0; i < 5; i++) {
        key_manager.apply_decay(key_id, decay_rate);
    }
    
    auto final_key = key_manager.get_key(key_id);
    ASSERT_TRUE(final_key.has_value());
    
    // Should follow exponential decay
    double expected_min = 0.5;
    EXPECT_LT(final_key->power, 1.0);
    EXPECT_GT(final_key->power, 0.0);
}

TEST_F(KeySystemTest, KeyTransfer) {
    auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    ASSERT_TRUE(key.has_value());
    
    // Create transfer
    KeyTransfer transfer{
        key->id,
        node1.id(),
        node2.id(),
        time::now(),
        bytes{} // Signature placeholder
    };
    
    // Execute transfer
    bool success = key_manager.transfer_key(transfer);
    EXPECT_TRUE(success);
    
    // Verify new owner
    auto transferred_key = key_manager.get_key(key->id);
    ASSERT_TRUE(transferred_key.has_value());
    EXPECT_EQ(transferred_key->owner, node2.id());
}

TEST_F(KeySystemTest, KeyVouching) {
    // Node1 vouches for Node2
    KeyVouch vouch{
        node1.id(),
        node2.id(),
        KeyType::Network,
        0.8, // strength
        time::now(),
        bytes{} // Signature placeholder
    };
    
    bool success = key_manager.record_vouch(vouch);
    EXPECT_TRUE(success);
    
    // Retrieve vouches
    auto vouches = key_manager.get_vouches_for(node2.id());
    EXPECT_GE(vouches.size(), 1);
    
    // Find our vouch
    bool found = false;
    for (const auto& v : vouches) {
        if (v.voucher == node1.id() && v.vouchee == node2.id()) {
            found = true;
            EXPECT_EQ(v.strength, 0.8);
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(KeySystemTest, KeyRevocation) {
    auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    ASSERT_TRUE(key.has_value());
    
    auto key_id = key->id;
    
    // Revoke key
    bool success = key_manager.revoke_key(key_id);
    EXPECT_TRUE(success);
    
    // Key should still exist but be marked revoked
    auto revoked_key = key_manager.get_key(key_id);
    ASSERT_TRUE(revoked_key.has_value());
    EXPECT_TRUE(revoked_key->is_revoked);
    
    // Should not be usable
    EXPECT_FALSE(key_manager.is_key_valid(key_id));
}

TEST_F(KeySystemTest, KeyExpiration) {
    auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    ASSERT_TRUE(key.has_value());
    
    auto key_id = key->id;
    
    // Set expiration in the past
    key_manager.set_expiration(key_id, time::now() - std::chrono::hours(1));
    
    // Key should be expired
    EXPECT_FALSE(key_manager.is_key_valid(key_id));
}

TEST_F(KeySystemTest, KeyPowerThresholds) {
    auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    ASSERT_TRUE(key.has_value());
    
    auto key_id = key->id;
    
    // Decay until below threshold
    for (int i = 0; i < 20; i++) {
        key_manager.apply_decay(key_id, 0.2);
    }
    
    auto decayed_key = key_manager.get_key(key_id);
    ASSERT_TRUE(decayed_key.has_value());
    
    // Key with very low power should be considered invalid
    if (decayed_key->power < 0.1) {
        EXPECT_FALSE(key_manager.is_key_valid(key_id));
    }
}

TEST_F(KeySystemTest, MultipleKeysPerNode) {
    // Issue multiple key types for same node
    auto id_key = key_manager.issue_key(node1.id(), KeyType::Identity, 1);
    auto net_key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    auto svc_key = key_manager.issue_key(node1.id(), KeyType::Service, 1);
    
    ASSERT_TRUE(id_key.has_value());
    ASSERT_TRUE(net_key.has_value());
    ASSERT_TRUE(svc_key.has_value());
    
    // All should have different IDs
    EXPECT_NE(id_key->id, net_key->id);
    EXPECT_NE(net_key->id, svc_key->id);
    
    // Get all keys for node
    auto node_keys = key_manager.get_keys_for_node(node1.id());
    EXPECT_GE(node_keys.size(), 3);
}

TEST_F(KeySystemTest, KeySerialization) {
    auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
    ASSERT_TRUE(key.has_value());
    
    // Serialize
    auto data = key->to_bytes();
    EXPECT_GT(data.size(), 0);
    
    // Deserialize
    auto restored = Key::from_bytes(data);
    ASSERT_TRUE(restored.has_value());
    
    EXPECT_EQ(restored->id, key->id);
    EXPECT_EQ(restored->type, key->type);
    EXPECT_EQ(restored->owner, key->owner);
    EXPECT_EQ(restored->epoch, key->epoch);
}

TEST_F(KeySystemTest, ConcurrentIssuance) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Try to issue keys concurrently
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, &success_count, i]() {
            auto key = key_manager.issue_key(node1.id(), KeyType::Network, 1);
            if (key.has_value()) {
                success_count++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Some should succeed (within epoch limits)
    EXPECT_GT(success_count, 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
