#include "core/thing/thing.hpp"
#include "crypto/blake3.hpp"
#include "crypto/ed25519.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>

using namespace cashew;
using namespace cashew::core;
using namespace cashew::crypto;

namespace fs = std::filesystem;

class ThingTest : public ::testing::Test {
protected:
    std::string test_dir = "./test_thing_data";
    
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
    
    // Helper to create valid ThingMetadata
    ThingMetadata create_metadata(const bytes& content, const std::string& name) {
        ThingMetadata meta;
        
        // Compute content hash
        Hash256 hash = Blake3::hash(content);
        meta.content_hash = ContentHash(hash);
        
        meta.name = name;
        meta.description = "Test thing";
        meta.type = ThingType::DOCUMENT;
        meta.size_bytes = content.size();
        meta.created_timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Create dummy node ID
        meta.creator_id = NodeID(Hash256{});
        
        meta.version = 1;
        
        // Create dummy signature (std::array<uint8_t, 64>)
        meta.creator_signature.fill(0);
        
        return meta;
    }
};

TEST_F(ThingTest, ThingCreation) {
    bytes content = {1, 2, 3, 4, 5};
    auto metadata = create_metadata(content, "Test Thing");
    
    auto thing_opt = Thing::create(content, metadata);
    
    ASSERT_TRUE(thing_opt.has_value());
    auto& thing = thing_opt.value();
    
    EXPECT_EQ(thing.data(), content);
    EXPECT_EQ(thing.metadata().name, "Test Thing");
    EXPECT_GT(thing.content_hash().hash.size(), 0);
}

TEST_F(ThingTest, ContentHashing) {
    bytes content1 = {1, 2, 3, 4, 5};
    bytes content2 = {1, 2, 3, 4, 5}; // Same content
    bytes content3 = {5, 4, 3, 2, 1}; // Different content
    
    auto meta1 = create_metadata(content1, "Thing 1");
    auto meta2 = create_metadata(content2, "Thing 2");
    auto meta3 = create_metadata(content3, "Thing 3");
    
    auto thing1 = Thing::create(content1, meta1);
    auto thing2 = Thing::create(content2, meta2);
    auto thing3 = Thing::create(content3, meta3);
    
    ASSERT_TRUE(thing1.has_value());
    ASSERT_TRUE(thing2.has_value());
    ASSERT_TRUE(thing3.has_value());
    
    // Same content should have same hash
    EXPECT_EQ(thing1->content_hash(), thing2->content_hash());
    
    // Different content should have different hash
    EXPECT_NE(thing1->content_hash(), thing3->content_hash());
}

TEST_F(ThingTest, BLAKE3Hashing) {
    bytes content = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    auto metadata = create_metadata(content, "Hello World");
    
    auto thing = Thing::create(content, metadata);
    ASSERT_TRUE(thing.has_value());
    
    // BLAKE3 hash should be 32 bytes
    EXPECT_EQ(thing->content_hash().hash.size(), 32);
    
    // Hash should be deterministic
    auto thing2 = Thing::create(content, metadata);
    ASSERT_TRUE(thing2.has_value());
    EXPECT_EQ(thing->content_hash(), thing2->content_hash());
}

TEST_F(ThingTest, SizeLimitEnforcement) {
    // Create content under max size (1MB should be fine)
    size_t safe_size = 1 * 1024 * 1024;
    bytes safe_content(safe_size, 0x42);
    auto safe_meta = create_metadata(safe_content, "Safe Thing");
    
    auto thing1 = Thing::create(safe_content, safe_meta);
    ASSERT_TRUE(thing1.has_value());
    EXPECT_EQ(thing1->size(), safe_size);
    
    // Create oversized content (500MB + 1 byte)
    // Note: This allocates large memory, may want to skip in CI
    size_t max_size = Thing::MAX_SIZE;
    bytes oversized_content(max_size + 1, 0x42);
    auto oversized_meta = create_metadata(oversized_content, "Oversized Thing");
    
    // Should fail to create
    auto thing2 = Thing::create(oversized_content, oversized_meta);
    EXPECT_FALSE(thing2.has_value());
}

TEST_F(ThingTest, EmptyContent) {
    bytes empty_content;
    auto metadata = create_metadata(empty_content, "Empty Thing");
    
    // Empty content should fail
    auto thing = Thing::create(empty_content, metadata);
    EXPECT_FALSE(thing.has_value());
}

TEST_F(ThingTest, ThingTypes) {
    bytes content = {1, 2, 3, 4, 5};
    
    // Test different types
    std::vector<ThingType> types = {
        ThingType::GAME,
        ThingType::DICTIONARY,
        ThingType::DATASET,
        ThingType::APP,
        ThingType::DOCUMENT,
        ThingType::MEDIA,
        ThingType::LIBRARY,
        ThingType::FORUM
    };
    
    for (auto type : types) {
        auto meta = create_metadata(content, "Test Thing");
        meta.type = type;
        
        auto thing = Thing::create(content, meta);
        ASSERT_TRUE(thing.has_value());
        EXPECT_EQ(thing->metadata().type, type);
    }
}

TEST_F(ThingTest, MetadataTags) {
    bytes content = {1, 2, 3};
    auto metadata = create_metadata(content, "Tagged Thing");
    
    metadata.tags = {"tag1", "tag2", "tag3", "important", "test"};
    
    auto thing = Thing::create(content, metadata);
    ASSERT_TRUE(thing.has_value());
    
    EXPECT_EQ(thing->metadata().tags.size(), 5);
    EXPECT_EQ(thing->metadata().tags[0], "tag1");
    EXPECT_EQ(thing->metadata().tags[4], "test");
}

TEST_F(ThingTest, MetadataSerialization) {
    bytes content = {1, 2, 3, 4, 5};
    auto metadata = create_metadata(content, "Serialization Test");
    metadata.tags = {"tag1", "tag2"};
    metadata.mime_type = "application/octet-stream";
    metadata.entry_point = "index.html";
    
    // Serialize
    auto serialized = metadata.serialize();
    EXPECT_GT(serialized.size(), 0);
    
    // Deserialization currently returns nullopt (not implemented)
    auto restored = ThingMetadata::deserialize(serialized);
    // TODO: When deserialization is implemented, verify fields match
}

TEST_F(ThingTest, ContentIntegrityVerification) {
    bytes content = {1, 2, 3, 4, 5};
    auto metadata = create_metadata(content, "Integrity Test");
    
    auto thing = Thing::create(content, metadata);
    ASSERT_TRUE(thing.has_value());
    
    // Verify original thing integrity
    EXPECT_TRUE(thing->verify_integrity());
}

TEST_F(ThingTest, HashMismatchDetection) {
    bytes content = {1, 2, 3, 4, 5};
    auto metadata = create_metadata(content, "Mismatch Test");
    
    // Corrupt the hash in metadata
    metadata.content_hash = ContentHash(Hash256{});  // Wrong hash
    
    // Should fail to create
    auto thing = Thing::create(content, metadata);
    EXPECT_FALSE(thing.has_value());
}

TEST_F(ThingTest, SizeMismatchDetection) {
    bytes content = {1, 2, 3, 4, 5};
    auto metadata = create_metadata(content, "Size Mismatch Test");
    
    // Wrong size in metadata
    metadata.size_bytes = 999;
    
    // Should fail to create
    auto thing = Thing::create(content, metadata);
    EXPECT_FALSE(thing.has_value());
}

TEST_F(ThingTest, LargeThingHandling) {
    // Create 10MB thing
    size_t size = 10 * 1024 * 1024;
    bytes large_content(size, 0x55);
    
    auto metadata = create_metadata(large_content, "Large Thing");
    metadata.tags = {"large", "test", "performance"};
    
    auto thing = Thing::create(large_content, metadata);
    ASSERT_TRUE(thing.has_value());
    
    EXPECT_EQ(thing->size(), size);
    EXPECT_EQ(thing->data().size(), size);
    
    // Verify hashing works for large content
    EXPECT_TRUE(thing->verify_integrity());
}

TEST_F(ThingTest, BinaryContentHandling) {
    // Binary content with all byte values 0-255
    bytes binary_content;
    for (int i = 0; i < 256; i++) {
        binary_content.push_back(static_cast<uint8_t>(i));
    }
    
    auto metadata = create_metadata(binary_content, "Binary Thing");
    auto thing = Thing::create(binary_content, metadata);
    
    ASSERT_TRUE(thing.has_value());
    EXPECT_EQ(thing->size(), 256);
    EXPECT_EQ(thing->data(), binary_content);
    
    // Verify hash computation works with binary data
    EXPECT_TRUE(thing->verify_integrity());
}

TEST_F(ThingTest, ChunkRetrieval) {
    bytes content = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto metadata = create_metadata(content, "Chunked Thing");
    
    auto thing = Thing::create(content, metadata);
    ASSERT_TRUE(thing.has_value());
    
    // Get chunk from start
    auto chunk1 = thing->get_chunk(0, 3);
    EXPECT_EQ(chunk1.size(), 3);
    EXPECT_EQ(chunk1[0], 0);
    EXPECT_EQ(chunk1[2], 2);
    
    // Get chunk from middle
    auto chunk2 = thing->get_chunk(5, 3);
    EXPECT_EQ(chunk2.size(), 3);
    EXPECT_EQ(chunk2[0], 5);
    EXPECT_EQ(chunk2[2], 7);
    
    // Get chunk at end
    auto chunk3 = thing->get_chunk(8, 10);  // Request more than available
    EXPECT_EQ(chunk3.size(), 2);  // Should only return what's available
    EXPECT_EQ(chunk3[0], 8);
    EXPECT_EQ(chunk3[1], 9);
    
    // Request beyond content
    auto chunk4 = thing->get_chunk(100, 10);
    EXPECT_EQ(chunk4.size(), 0);
}

TEST_F(ThingTest, MIMETypeHandling) {
    bytes content = {1, 2, 3};
    auto metadata = create_metadata(content, "MIME Test");
    
    metadata.mime_type = "text/html";
    metadata.entry_point = "index.html";
    
    auto thing = Thing::create(content, metadata);
    ASSERT_TRUE(thing.has_value());
    
    EXPECT_EQ(thing->metadata().mime_type, "text/html");
    EXPECT_EQ(thing->metadata().entry_point, "index.html");
}

TEST_F(ThingTest, Versioning) {
    bytes content_v1 = {1, 2, 3};
    bytes content_v2 = {1, 2, 3, 4, 5};
    
    auto meta_v1 = create_metadata(content_v1, "Version 1");
    meta_v1.version = 1;
    
    auto meta_v2 = create_metadata(content_v2, "Version 2");
    meta_v2.version = 2;
    
    auto thing_v1 = Thing::create(content_v1, meta_v1);
    auto thing_v2 = Thing::create(content_v2, meta_v2);
    
    ASSERT_TRUE(thing_v1.has_value());
    ASSERT_TRUE(thing_v2.has_value());
    
    // Different versions should have different hashes
    EXPECT_NE(thing_v1->content_hash(), thing_v2->content_hash());
    EXPECT_EQ(thing_v1->metadata().version, 1);
    EXPECT_EQ(thing_v2->metadata().version, 2);
}

TEST_F(ThingTest, ContentImmutability) {
    bytes content = {1, 2, 3, 4, 5};
    auto metadata = create_metadata(content, "Immutable Thing");
    
    auto thing = Thing::create(content, metadata);
    ASSERT_TRUE(thing.has_value());
    
    auto original_hash = thing->content_hash();
    
    // Content should be const-accessible only
    const auto& data = thing->data();
    EXPECT_EQ(data.size(), 5);
    
    // Hash should remain the same
    EXPECT_EQ(thing->content_hash(), original_hash);
    EXPECT_TRUE(thing->verify_integrity());
}

TEST_F(ThingTest, MaxSizeThing) {
    // Test creating a Thing at exactly the maximum size
    // Note: This test allocates 500MB of memory
    // Uncomment to test in production environment
    /*
    size_t max_size = Thing::MAX_SIZE;
    bytes max_content(max_size, 0xAA);
    
    auto metadata = create_metadata(max_content, "Max Size Thing");
    auto thing = Thing::create(max_content, metadata);
    
    ASSERT_TRUE(thing.has_value());
    EXPECT_EQ(thing->size(), max_size);
    EXPECT_TRUE(thing->verify_integrity());
    */
    
    // For now, just verify the constant
    EXPECT_EQ(Thing::MAX_SIZE, 500 * 1024 * 1024);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
