#include "storage/storage.hpp"
#include "core/thing/thing.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>
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
    auto hash = storage.store(test_data);
    
    ASSERT_TRUE(hash.has_value());
    
    // Retrieve data
    auto retrieved = storage.retrieve(hash.value());
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(test_data, retrieved.value());
}

TEST_F(StorageTest, ContentAddressing) {
    Storage storage(test_dir);
    
    bytes data1 = {1, 2, 3};
    bytes data2 = {1, 2, 3}; // Same content
    bytes data3 = {4, 5, 6}; // Different content
    
    auto hash1 = storage.store(data1);
    auto hash2 = storage.store(data2);
    auto hash3 = storage.store(data3);
    
    // Same content should have same hash
    EXPECT_EQ(hash1.value(), hash2.value());
    // Different content should have different hash
    EXPECT_NE(hash1.value(), hash3.value());
}

TEST_F(StorageTest, Deduplication) {
    Storage storage(test_dir);
    
    bytes data = {1, 2, 3, 4, 5};
    
    // Store same data multiple times
    auto hash1 = storage.store(data);
    auto hash2 = storage.store(data);
    auto hash3 = storage.store(data);
    
    // Should all return same hash
    EXPECT_EQ(hash1.value(), hash2.value());
    EXPECT_EQ(hash2.value(), hash3.value());
    
    // Storage size should not increase
    auto stats = storage.get_stats();
    EXPECT_EQ(stats.unique_items, 1);
}

TEST_F(StorageTest, LargeContent) {
    Storage storage(test_dir);
    
    // Create 1MB of data
    bytes large_data(1024 * 1024, 0x42);
    
    auto hash = storage.store(large_data);
    ASSERT_TRUE(hash.has_value());
    
    auto retrieved = storage.retrieve(hash.value());
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(large_data.size(), retrieved.value().size());
    EXPECT_EQ(large_data, retrieved.value());
}

TEST_F(StorageTest, Chunking) {
    Storage storage(test_dir);
    
    // Create 10MB of data to trigger chunking
    bytes large_data(10 * 1024 * 1024, 0x55);
    
    auto hash = storage.store(large_data);
    ASSERT_TRUE(hash.has_value());
    
    // Verify chunking occurred
    auto stats = storage.get_stats();
    EXPECT_GT(stats.chunks, 1);
    
    // Verify retrieval works
    auto retrieved = storage.retrieve(hash.value());
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(large_data.size(), retrieved.value().size());
}

TEST_F(StorageTest, QuotaManagement) {
    // Create storage with 1MB quota
    Storage storage(test_dir, 1024 * 1024);
    
    // Store 500KB
    bytes data1(500 * 1024, 0x11);
    auto hash1 = storage.store(data1);
    ASSERT_TRUE(hash1.has_value());
    
    // Store another 500KB (should succeed)
    bytes data2(500 * 1024, 0x22);
    auto hash2 = storage.store(data2);
    ASSERT_TRUE(hash2.has_value());
    
    // Try to store more (should fail or trigger GC)
    bytes data3(200 * 1024, 0x33);
    auto hash3 = storage.store(data3);
    
    // Either rejected or old data was cleaned
    auto stats = storage.get_stats();
    EXPECT_LE(stats.total_size, 1024 * 1024);
}

TEST_F(StorageTest, MetadataStorage) {
    Storage storage(test_dir);
    
    bytes data = {1, 2, 3, 4, 5};
    auto hash = storage.store(data);
    ASSERT_TRUE(hash.has_value());
    
    // Store metadata
    nlohmann::json metadata;
    metadata["title"] = "Test Thing";
    metadata["author"] = "Alice";
    
    bool result = storage.store_metadata(hash.value(), metadata);
    EXPECT_TRUE(result);
    
    // Retrieve metadata
    auto retrieved_meta = storage.get_metadata(hash.value());
    ASSERT_TRUE(retrieved_meta.has_value());
    EXPECT_EQ(metadata["title"], retrieved_meta.value()["title"]);
    EXPECT_EQ(metadata["author"], retrieved_meta.value()["author"]);
}

TEST_F(StorageTest, GarbageCollection) {
    Storage storage(test_dir, 1024 * 1024); // 1MB quota
    
    std::vector<Hash256> hashes;
    
    // Fill storage
    for (int i = 0; i < 10; i++) {
        bytes data(100 * 1024, static_cast<uint8_t>(i));
        auto hash = storage.store(data);
        if (hash.has_value()) {
            hashes.push_back(hash.value());
        }
    }
    
    // Mark some as accessed (to keep them)
    storage.mark_accessed(hashes[0]);
    storage.mark_accessed(hashes[1]);
    
    // Run garbage collection
    storage.garbage_collect();
    
    // Recently accessed items should still exist
    EXPECT_TRUE(storage.exists(hashes[0]));
    EXPECT_TRUE(storage.exists(hashes[1]));
    
    // Some old items may have been cleaned
    auto stats = storage.get_stats();
    EXPECT_LE(stats.total_size, 1024 * 1024);
}

TEST_F(StorageTest, NonExistentRetrieval) {
    Storage storage(test_dir);
    
    // Try to retrieve non-existent hash
    Hash256 fake_hash;
    std::fill(fake_hash.begin(), fake_hash.end(), 0xFF);
    
    auto result = storage.retrieve(fake_hash);
    EXPECT_FALSE(result.has_value());
}

TEST_F(StorageTest, StorageStats) {
    Storage storage(test_dir);
    
    // Store some data
    bytes data1(1024, 0x11);
    bytes data2(2048, 0x22);
    bytes data3(512, 0x33);
    
    storage.store(data1);
    storage.store(data2);
    storage.store(data3);
    
    auto stats = storage.get_stats();
    
    EXPECT_EQ(stats.unique_items, 3);
    EXPECT_GT(stats.total_size, 3500);
    EXPECT_GE(stats.chunks, 3);
}

TEST_F(StorageTest, ConcurrentAccess) {
    Storage storage(test_dir);
    
    // Store initial data
    bytes data(1024, 0x42);
    auto hash = storage.store(data);
    ASSERT_TRUE(hash.has_value());
    
    // Simulate concurrent reads
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&storage, &hash, &success_count]() {
            auto result = storage.retrieve(hash.value());
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
