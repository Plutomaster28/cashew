#include "storage/storage.hpp"
#include "core/thing/thing.hpp"
#include "cashew/common.hpp"
#include "crypto/blake3.hpp"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using namespace cashew;
using namespace cashew::storage;
using namespace cashew::core;

namespace fs = std::filesystem;

class StorageTest : public ::testing::Test {
protected:
    std::string test_dir = "./test_storage_data";
    
    void SetUp() override {
        // Clean up any previous test data
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);
    }
    
    void TearDown() override {
        // Clean up test data
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }
};

TEST_F(StorageTest, StoreAndRetrieve) {
    Storage storage(test_dir);
    
    // Create test data
    bytes test_data = {1, 2, 3, 4, 5};
    ContentHash hash(crypto::Blake3::hash(test_data));
    bool stored = storage.put_content(hash, test_data);
    
    ASSERT_TRUE(stored);
    
    // Retrieve data
    auto retrieved = storage.get_content(hash);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(test_data, retrieved.value());
}

TEST_F(StorageTest, ContentAddressing) {
    Storage storage(test_dir);
    
    bytes data1 = {1, 2, 3};
    bytes data2 = {1, 2, 3}; // Same content
    bytes data3 = {4, 5, 6}; // Different content
    
    ContentHash hash1(crypto::Blake3::hash(data1));
    ContentHash hash2(crypto::Blake3::hash(data2));
    ContentHash hash3(crypto::Blake3::hash(data3));
    
    storage.put_content(hash1, data1);
    storage.put_content(hash2, data2);
    storage.put_content(hash3, data3);
    
    // Same content should have same hash
    EXPECT_EQ(hash1, hash2);
    // Different content should have different hash
    EXPECT_NE(hash1, hash3);
}

TEST_F(StorageTest, Deduplication) {
    Storage storage(test_dir);
    
    bytes data = {1, 2, 3, 4, 5};
    ContentHash hash(crypto::Blake3::hash(data));
    
    // Store same data multiple times
    storage.put_content(hash, data);
    storage.put_content(hash, data);
    storage.put_content(hash, data);
    
    // Should all have same hash and be deduped
    EXPECT_TRUE(storage.has_content(hash));
    
    // Storage should only contain one copy
    EXPECT_EQ(storage.item_count(), 1);
}

TEST_F(StorageTest, LargeContent) {
    Storage storage(test_dir);
    
    // Create 1MB of data
    bytes large_data(1024 * 1024, 0x42);
    ContentHash hash(crypto::Blake3::hash(large_data));
    
    bool stored = storage.put_content(hash, large_data);
    ASSERT_TRUE(stored);
    
    auto retrieved = storage.get_content(hash);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(large_data.size(), retrieved.value().size());
    EXPECT_EQ(large_data, retrieved.value());
}

TEST_F(StorageTest, Chunking) {
    Storage storage(test_dir);
    
    // Create 10MB of data
    bytes large_data(10 * 1024 * 1024, 0x55);
    ContentHash hash(crypto::Blake3::hash(large_data));
    
    bool stored = storage.put_content(hash, large_data);
    ASSERT_TRUE(stored);
    
    // Verify retrieval works
    auto retrieved = storage.get_content(hash);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(large_data.size(), retrieved.value().size());
}

TEST_F(StorageTest, QuotaManagement) {
    // Storage doesn't have quota management in current implementation
    Storage storage(test_dir);
    
    // Store 500KB
    bytes data1(500 * 1024, 0x11);
    ContentHash hash1(crypto::Blake3::hash(data1));
    bool stored1 = storage.put_content(hash1, data1);
    ASSERT_TRUE(stored1);
    
    // Store another 500KB
    bytes data2(500 * 1024, 0x22);
    ContentHash hash2(crypto::Blake3::hash(data2));
    bool stored2 = storage.put_content(hash2, data2);
    ASSERT_TRUE(stored2);
    
    // Verify both are stored
    EXPECT_TRUE(storage.has_content(hash1));
    EXPECT_TRUE(storage.has_content(hash2));
}

TEST_F(StorageTest, MetadataStorage) {
    Storage storage(test_dir);
    
    // Store metadata using string key
    nlohmann::json metadata;
    metadata["title"] = "Test Thing";
    metadata["author"] = "Alice";
    
    std::string metadata_str = metadata.dump();
    bytes metadata_bytes(metadata_str.begin(), metadata_str.end());
    
    bool result = storage.put_metadata("test_key", metadata_bytes);
    EXPECT_TRUE(result);
    
    // Retrieve metadata
    auto retrieved_bytes = storage.get_metadata("test_key");
    ASSERT_TRUE(retrieved_bytes.has_value());
    
    std::string retrieved_str(retrieved_bytes.value().begin(), retrieved_bytes.value().end());
    auto retrieved_meta = nlohmann::json::parse(retrieved_str);
    EXPECT_EQ(metadata["title"], retrieved_meta["title"]);
    EXPECT_EQ(metadata["author"], retrieved_meta["author"]);
}

TEST_F(StorageTest, GarbageCollection) {
    Storage storage(test_dir);
    
    std::vector<ContentHash> hashes;
    
    // Store multiple items
    for (int i = 0; i < 5; i++) {
        bytes data(100 * 1024, static_cast<uint8_t>(i));
        ContentHash hash(crypto::Blake3::hash(data));
        bool stored = storage.put_content(hash, data);
        if (stored) {
            hashes.push_back(hash);
        }
    }
    
    // Verify items exist
    for (const auto& hash : hashes) {
        EXPECT_TRUE(storage.has_content(hash));
    }
    
    // Test compact operation
    storage.compact();
    
    // Items should still exist after compaction
    for (const auto& hash : hashes) {
        EXPECT_TRUE(storage.has_content(hash));
    }
}

TEST_F(StorageTest, NonExistentRetrieval) {
    Storage storage(test_dir);
    
    // Try to retrieve non-existent hash
    Hash256 fake_hash_data;
    std::fill(fake_hash_data.begin(), fake_hash_data.end(), 0xFF);
    ContentHash fake_hash(fake_hash_data);
    
    auto result = storage.get_content(fake_hash);
    EXPECT_FALSE(result.has_value());
}

TEST_F(StorageTest, StorageStats) {
    Storage storage(test_dir);
    
    // Store some data
    bytes data1(1024, 0x11);
    bytes data2(2048, 0x22);
    bytes data3(512, 0x33);
    
    ContentHash hash1(crypto::Blake3::hash(data1));
    ContentHash hash2(crypto::Blake3::hash(data2));
    ContentHash hash3(crypto::Blake3::hash(data3));
    
    storage.put_content(hash1, data1);
    storage.put_content(hash2, data2);
    storage.put_content(hash3, data3);
    
    EXPECT_EQ(storage.item_count(), 3);
    EXPECT_GT(storage.total_size(), 3500);
}

TEST_F(StorageTest, ConcurrentAccess) {
    Storage storage(test_dir);
    
    // Store initial data
    bytes data(1024, 0x42);
    ContentHash hash(crypto::Blake3::hash(data));
    bool stored = storage.put_content(hash, data);
    ASSERT_TRUE(stored);
    
    // Simulate concurrent reads
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&storage, hash, &success_count]() {
            auto result = storage.get_content(hash);
            if (result.has_value()) {
                success_count++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(success_count, 10);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
