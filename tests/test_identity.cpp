#include <gtest/gtest.h>
#include "../src/core/node/node_identity.hpp"
#include "../src/utils/logger.hpp"
#include <filesystem>

using namespace cashew::core;
using cashew::utils::Logger;

class NodeIdentityTestFixture : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Initialize logger once for all tests
        Logger::init("error", false);  // Only show errors to keep test output clean
    }
};

TEST_F(NodeIdentityTestFixture, Generation) {
    auto identity = NodeIdentity::generate();
    
    EXPECT_NE(identity.id().to_string(), "");
    EXPECT_EQ(identity.public_key().size(), 32);
}

TEST_F(NodeIdentityTestFixture, SignAndVerify) {
    auto identity = NodeIdentity::generate();
    
    cashew::bytes message = {'t', 'e', 's', 't', ' ', 'm', 'e', 's', 's', 'a', 'g', 'e'};
    auto signature = identity.sign(message);
    
    EXPECT_TRUE(identity.verify(message, signature));
}

TEST_F(NodeIdentityTestFixture, SaveAndLoad) {
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

TEST_F(NodeIdentityTestFixture, SaveAndLoadEncrypted) {
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

TEST_F(NodeIdentityTestFixture, KeyRotation) {
    auto original = NodeIdentity::generate();
    
    // Rotate once
    auto rotated = original.rotate("Regular scheduled rotation");
    
    // Verify rotation history
    EXPECT_EQ(rotated.rotation_history().size(), 1);
    EXPECT_TRUE(rotated.verify_rotation_chain());
    
    // Genesis key should be the original key
    EXPECT_EQ(rotated.get_genesis_key(), original.public_key());
    
    // Rotate again
    auto rotated2 = rotated.rotate("Security update");
    
    EXPECT_EQ(rotated2.rotation_history().size(), 2);
    EXPECT_TRUE(rotated2.verify_rotation_chain());
    
    // Genesis key should still be the original key
    EXPECT_EQ(rotated2.get_genesis_key(), original.public_key());
}

TEST_F(NodeIdentityTestFixture, RotationCertificateVerification) {
    auto identity = NodeIdentity::generate();
    auto rotated = identity.rotate("Test rotation");
    
    // Get the rotation certificate
    const auto& certs = rotated.rotation_history();
    ASSERT_EQ(certs.size(), 1);
    
    const auto& cert = certs[0];
    
    // Verify certificate properties
    EXPECT_EQ(cert.old_public_key, identity.public_key());
    EXPECT_EQ(cert.new_public_key, rotated.public_key());
    EXPECT_EQ(cert.reason, "Test rotation");
    
    // Verify certificate signature
    EXPECT_TRUE(cert.verify());
}

TEST_F(NodeIdentityTestFixture, SaveAndLoadWithRotation) {
    auto original = NodeIdentity::generate();
    auto rotated = original.rotate("First rotation");
    auto rotated2 = rotated.rotate("Second rotation");
    
    auto temp_file = std::filesystem::temp_directory_path() / "test_identity_rotated.dat";
    std::string password = "test_password";
    
    // Save rotated identity
    rotated2.save(temp_file, password);
    
    // Load it back
    auto loaded = NodeIdentity::load(temp_file, password);
    
    // Verify rotation history was preserved
    EXPECT_EQ(loaded.rotation_history().size(), 2);
    EXPECT_TRUE(loaded.verify_rotation_chain());
    
    // Verify genesis key
    EXPECT_EQ(loaded.get_genesis_key(), original.public_key());
    
    // Verify current identity
    EXPECT_EQ(loaded.id(), rotated2.id());
    EXPECT_EQ(loaded.public_key(), rotated2.public_key());
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_F(NodeIdentityTestFixture, MultipleRotations) {
    auto identity = NodeIdentity::generate();
    
    // Perform multiple rotations
    for (int i = 0; i < 5; ++i) {
        identity = identity.rotate("Rotation " + std::to_string(i + 1));
    }
    
    // Verify chain
    EXPECT_EQ(identity.rotation_history().size(), 5);
    EXPECT_TRUE(identity.verify_rotation_chain());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
