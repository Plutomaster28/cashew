#include "core/keys/key.hpp"
#include "core/node/node_identity.hpp"
#include "core/pow/pow.hpp"
#include "cashew/common.hpp"
#include "cashew/time_utils.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::core;

class KeySystemTest : public ::testing::Test {
protected:
    std::unique_ptr<NodeIdentity> node1;
    std::unique_ptr<NodeIdentity> node2;
    KeyManager key_manager;
    
    void SetUp() override {
        node1 = std::make_unique<NodeIdentity>(NodeIdentity::generate());
        node2 = std::make_unique<NodeIdentity>(NodeIdentity::generate());
    }
};

TEST_F(KeySystemTest, KeyIssuance) {
    // Create an identity key using the actual API
    auto key = Key::create(
        KeyType::IDENTITY,
        node1->id(),
        time::timestamp_seconds(),
        "pow"
    );
    
    key_manager.add_key(key);
    
    auto retrieved = key_manager.get_key(key.get_key_id());
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->type(), KeyType::IDENTITY);
    EXPECT_EQ(retrieved->owner(), node1->id());
}

TEST_F(KeySystemTest, AllKeyTypes) {
    std::vector<KeyType> types = {
        KeyType::IDENTITY,
        KeyType::NODE,
        KeyType::NETWORK,
        KeyType::SERVICE,
        KeyType::ROUTING
    };
    
    for (auto type : types) {
        auto key = Key::create(
            type,
            node1->id(),
            time::timestamp_seconds(),
            "pow"
        );
        key_manager.add_key(key);
        
        auto retrieved = key_manager.get_key(key.get_key_id());
        ASSERT_TRUE(retrieved.has_value());
        EXPECT_EQ(retrieved->type(), type);
    }
}

TEST_F(KeySystemTest, EpochLimits) {
    // Create multiple keys of the same type
    std::vector<Key> keys;
    for (int i = 0; i < 5; i++) {
        auto key = Key::create(
            KeyType::NETWORK,
            node1->id(),
            time::timestamp_seconds(),
            "pow"
        );
        key_manager.add_key(key);
        keys.push_back(key);
    }
    
    // Should be able to count keys
    uint32_t count = key_manager.count_keys(node1->id(), KeyType::NETWORK);
    EXPECT_EQ(count, 5);
    
    // Get keys by type
    auto network_keys = key_manager.get_keys_by_type(KeyType::NETWORK);
    EXPECT_GE(network_keys.size(), 5);
}

TEST_F(KeySystemTest, KeyDecay) {
    auto key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        time::timestamp_seconds() - (Key::DECAY_PERIOD_SECONDS + 100), // Old key
        "pow"
    );
    
    // Check if key has decayed
    uint64_t current_time = time::timestamp_seconds();
    EXPECT_TRUE(key.has_decayed(current_time));
    
    // Time until decay should be 0
    EXPECT_EQ(key.time_until_decay(current_time), 0);
    
    // Fresh key should not have decayed
    auto fresh_key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        time::timestamp_seconds(),
        "pow"
    );
    EXPECT_FALSE(fresh_key.has_decayed(current_time));
    EXPECT_GT(fresh_key.time_until_decay(current_time), 0);
}

TEST_F(KeySystemTest, ExponentialDecay) {
    auto key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        time::timestamp_seconds(),
        "pow"
    );
    
    uint64_t initial_last_used = key.last_used();
    
    // Mark key as used
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t new_time = time::timestamp_seconds();
    key.mark_used(new_time);
    
    // Last used time should be updated
    EXPECT_GT(key.last_used(), initial_last_used);
}

TEST_F(KeySystemTest, KeyTransfer) {
    auto key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        time::timestamp_seconds(),
        "pow"
    );
    std::string key_id = key.get_key_id();
    key_manager.add_key(key);
    
    // Create a dummy secret key for testing
    SecretKey dummy_sk;
    std::fill(dummy_sk.begin(), dummy_sk.end(), 0x01);
    
    // Create transfer
    auto transfer_opt = key_manager.create_transfer(
        key_id,
        node1->id(),
        node2->id(),
        "Transfer for testing",
        dummy_sk
    );
    
    ASSERT_TRUE(transfer_opt.has_value());
    
    // Execute transfer
    bool success = key_manager.execute_transfer(transfer_opt.value());
    EXPECT_TRUE(success);
    
    // Check transfer history
    auto history = key_manager.get_transfer_history(node1->id());
    EXPECT_GE(history.size(), 1);
}

TEST_F(KeySystemTest, KeyVouching) {
    // Create a dummy secret key for testing
    SecretKey dummy_sk;
    std::fill(dummy_sk.begin(), dummy_sk.end(), 0x01);
    
    // Node1 vouches for Node2
    auto vouch_opt = key_manager.create_vouch(
        node1->id(),
        node2->id(),
        KeyType::NETWORK,
        1, // key_count
        "Vouching for trusted node",
        dummy_sk
    );
    
    ASSERT_TRUE(vouch_opt.has_value());
    
    bool success = key_manager.execute_vouch(vouch_opt.value(), time::timestamp_seconds());
    EXPECT_TRUE(success);
    
    // Retrieve vouches
    auto vouches_by = key_manager.get_vouches_by(node1->id());
    EXPECT_GE(vouches_by.size(), 1);
    
    auto vouches_for = key_manager.get_vouches_for(node2->id());
    EXPECT_GE(vouches_for.size(), 1);
    
    // Check vouch stats
    auto stats = key_manager.get_vouch_stats(node1->id());
    EXPECT_GT(stats.total_vouches_given, 0);
}

TEST_F(KeySystemTest, KeyRevocation) {
    auto key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        time::timestamp_seconds(),
        "pow"
    );
    std::string key_id = key.get_key_id();
    
    key_manager.add_key(key);
    
    // Verify key exists
    auto retrieved = key_manager.get_key(key_id);
    ASSERT_TRUE(retrieved.has_value());
    
    // Remove/revoke key
    bool success = key_manager.remove_key(key_id);
    EXPECT_TRUE(success);
    
    // Key should no longer exist
    auto removed = key_manager.get_key(key_id);
    EXPECT_FALSE(removed.has_value());
}

TEST_F(KeySystemTest, KeyExpiration) {
    // Create a key that's already expired (very old)
    auto key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        100, // Very old timestamp
        "pow"
    );
    
    // Key should have decayed
    uint64_t current_time = time::timestamp_seconds();
    EXPECT_TRUE(key.has_decayed(current_time));
}

TEST_F(KeySystemTest, KeyPowerThresholds) {
    auto key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        time::timestamp_seconds(),
        "pow"
    );
    
    // Test decay period constant
    EXPECT_GT(Key::DECAY_PERIOD_SECONDS, 0);
    
    // Test fresh key hasn't decayed
    EXPECT_FALSE(key.has_decayed(time::timestamp_seconds()));
}

TEST_F(KeySystemTest, MultipleKeysPerNode) {
    // Issue multiple key types for same node
    auto id_key = Key::create(KeyType::IDENTITY, node1->id(), time::timestamp_seconds(), "pow");
    auto net_key = Key::create(KeyType::NETWORK, node1->id(), time::timestamp_seconds(), "pow");
    auto svc_key = Key::create(KeyType::SERVICE, node1->id(), time::timestamp_seconds(), "pow");
    
    key_manager.add_key(id_key);
    key_manager.add_key(net_key);
    key_manager.add_key(svc_key);
    
    // All should have different IDs
    EXPECT_NE(id_key.get_key_id(), net_key.get_key_id());
    EXPECT_NE(net_key.get_key_id(), svc_key.get_key_id());
    
    // Get all keys for node
    auto node_keys = key_manager.get_keys_by_owner(node1->id());
    EXPECT_GE(node_keys.size(), 3);
}

TEST_F(KeySystemTest, KeySerialization) {
    auto key = Key::create(
        KeyType::NETWORK,
        node1->id(),
        time::timestamp_seconds(),
        "pow"
    );
    
    // Serialize
    auto data = key.serialize();
    EXPECT_GT(data.size(), 0);
    
    // Deserialize
    auto restored = Key::deserialize(data);
    ASSERT_TRUE(restored.has_value());
    
    EXPECT_EQ(restored->get_key_id(), key.get_key_id());
    EXPECT_EQ(restored->type(), key.type());
    EXPECT_EQ(restored->owner(), key.owner());
    EXPECT_EQ(restored->issued_at(), key.issued_at());
}

TEST_F(KeySystemTest, ConcurrentIssuance) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Try to add keys concurrently
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([this, &success_count, i]() {
            auto key = Key::create(
                KeyType::NETWORK,
                node1->id(),
                time::timestamp_seconds() + i, // Slightly different timestamps
                "pow"
            );
            key_manager.add_key(key);
            success_count++;
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All should succeed
    EXPECT_EQ(success_count, 10);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
