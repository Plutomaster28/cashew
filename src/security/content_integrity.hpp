#pragma once

#include "cashew/common.hpp"
#include "crypto/blake3.hpp"
#include <vector>
#include <optional>
#include <string>

namespace cashew::security {

/**
 * ContentIntegrityChecker - Verifies content hasn't been tampered with
 */
class ContentIntegrityChecker {
public:
    ContentIntegrityChecker() = default;
    ~ContentIntegrityChecker() = default;
    
    /**
     * Verify content matches its expected hash
     */
    struct VerificationResult {
        bool is_valid;
        Hash256 expected_hash;
        Hash256 actual_hash;
        size_t content_size;
        std::string error_message;
    };
    
    /**
     * Verify content integrity by comparing hash
     */
    static VerificationResult verify_content(
        const std::vector<uint8_t>& content,
        const Hash256& expected_hash
    );
    
    /**
     * Verify content integrity from storage
     */
    static VerificationResult verify_content_hash(
        const ContentHash& content_hash,
        const std::vector<uint8_t>& content
    );
    
    /**
     * Calculate and verify Merkle tree for chunked content
     */
    struct MerkleNode {
        Hash256 hash;
        size_t offset;
        size_t length;
        std::vector<Hash256> children;
    };
    
    static MerkleNode build_merkle_tree(
        const std::vector<uint8_t>& content,
        size_t chunk_size = 64 * 1024  // 64KB chunks
    );
    
    static bool verify_merkle_tree(
        const std::vector<uint8_t>& content,
        const MerkleNode& root,
        size_t chunk_size = 64 * 1024
    );
    
    /**
     * Verify partial content (for range requests)
     */
    static bool verify_chunk(
        const std::vector<uint8_t>& chunk,
        const Hash256& expected_chunk_hash
    );
    
    /**
     * Generate integrity metadata for Thing
     */
    struct IntegrityMetadata {
        Hash256 content_hash;
        size_t content_size;
        MerkleNode merkle_root;
        uint64_t created_at;
        std::vector<Hash256> chunk_hashes;  // For verification
    };
    
    static IntegrityMetadata generate_metadata(
        const std::vector<uint8_t>& content,
        size_t chunk_size = 64 * 1024
    );
    
    static bool verify_metadata(
        const std::vector<uint8_t>& content,
        const IntegrityMetadata& metadata
    );
};

} // namespace cashew::security
