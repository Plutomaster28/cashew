#include <gtest/gtest.h>
#include "../src/crypto/ed25519.hpp"
#include "../src/crypto/blake3.hpp"
#include "../src/crypto/chacha20poly1305.hpp"

using namespace cashew::crypto;

TEST(Ed25519Test, KeypairGeneration) {
    auto [pk, sk] = Ed25519::generate_keypair();
    EXPECT_EQ(pk.size(), 32);
    EXPECT_EQ(sk.size(), 32);
}

TEST(Ed25519Test, SignAndVerify) {
    auto [pk, sk] = Ed25519::generate_keypair();
    
    cashew::bytes message = {'H', 'e', 'l', 'l', 'o', ' ', 'C', 'a', 's', 'h', 'e', 'w'};
    auto signature = Ed25519::sign(message, sk);
    
    EXPECT_TRUE(Ed25519::verify(message, signature, pk));
    
    // Tampered message should fail
    message[0] = 'h';
    EXPECT_FALSE(Ed25519::verify(message, signature, pk));
}

TEST(Ed25519Test, HexConversion) {
    auto [pk, sk] = Ed25519::generate_keypair();
    
    auto hex = Ed25519::public_key_to_hex(pk);
    auto parsed = Ed25519::public_key_from_hex(hex);
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(pk, *parsed);
}

TEST(Blake3Test, BasicHashing) {
    cashew::bytes data = {'t', 'e', 's', 't'};
    auto hash1 = Blake3::hash(data);
    auto hash2 = Blake3::hash(data);
    
    // Same input should produce same hash
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.size(), 32);
}

TEST(Blake3Test, DifferentInputs) {
    cashew::bytes data1 = {'t', 'e', 's', 't', '1'};
    cashew::bytes data2 = {'t', 'e', 's', 't', '2'};
    
    auto hash1 = Blake3::hash(data1);
    auto hash2 = Blake3::hash(data2);
    
    EXPECT_NE(hash1, hash2);
}

TEST(Blake3Test, HexConversion) {
    auto hash = Blake3::hash("test");
    auto hex = Blake3::hash_to_hex(hash);
    auto parsed = Blake3::hash_from_hex(hex);
    
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(hash, *parsed);
}

TEST(ChaCha20Poly1305Test, EncryptDecrypt) {
    cashew::bytes plaintext = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    auto key = ChaCha20Poly1305::generate_key();
    auto nonce = ChaCha20Poly1305::generate_nonce();
    
    auto ciphertext = ChaCha20Poly1305::encrypt(plaintext, key, nonce);
    auto decrypted = ChaCha20Poly1305::decrypt(ciphertext, key, nonce);
    
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(plaintext, *decrypted);
}

TEST(ChaCha20Poly1305Test, WrongKey) {
    cashew::bytes plaintext = {'s', 'e', 'c', 'r', 'e', 't'};
    auto key1 = ChaCha20Poly1305::generate_key();
    auto key2 = ChaCha20Poly1305::generate_key();
    auto nonce = ChaCha20Poly1305::generate_nonce();
    
    auto ciphertext = ChaCha20Poly1305::encrypt(plaintext, key1, nonce);
    auto decrypted = ChaCha20Poly1305::decrypt(ciphertext, key2, nonce);
    
    EXPECT_FALSE(decrypted.has_value()); // Should fail with wrong key
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
