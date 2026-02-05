#include <gtest/gtest.h>
#include "../src/core/node/node_identity.hpp"
#include <filesystem>

using namespace cashew::core;

TEST(NodeIdentityTest, Generation) {
    auto identity = NodeIdentity::generate();
    
    EXPECT_NE(identity.id().to_string(), "");
    EXPECT_EQ(identity.public_key().size(), 32);
}

TEST(NodeIdentityTest, SignAndVerify) {
    auto identity = NodeIdentity::generate();
    
    cashew::bytes message = {'t', 'e', 's', 't', ' ', 'm', 'e', 's', 's', 'a', 'g', 'e'};
    auto signature = identity.sign(message);
    
    EXPECT_TRUE(identity.verify(message, signature));
}

TEST(NodeIdentityTest, SaveAndLoad) {
    auto original = NodeIdentity::generate();
    auto temp_file = std::filesystem::temp_directory_path() / "test_identity.dat";
    
    // Save
    original.save(temp_file);
    
    // Load
    auto loaded = NodeIdentity::load(temp_file);
    
    // Verify same identity
    EXPECT_EQ(original.id(), loaded.id());
    EXPECT_EQ(original.public_key(), loaded.public_key());
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST(NodeIdentityTest, SaveAndLoadEncrypted) {
    auto original = NodeIdentity::generate();
    auto temp_file = std::filesystem::temp_directory_path() / "test_identity_encrypted.dat";
    std::string password = "super_secret_password";
    
    // Save encrypted
    original.save(temp_file, password);
    
    // Load with correct password
    auto loaded = NodeIdentity::load(temp_file, password);
    EXPECT_EQ(original.id(), loaded.id());
    
    // Try loading with wrong password - should throw
    EXPECT_THROW(
        NodeIdentity::load(temp_file, "wrong_password"),
        std::runtime_error
    );
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
