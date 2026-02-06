#include "security/content_integrity.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace cashew::security {

ContentIntegrityChecker::VerificationResult ContentIntegrityChecker::verify_content(
    const std::vector<uint8_t>& content,
    const Hash256& expected_hash
) {
    VerificationResult result;
    result.expected_hash = expected_hash;
    result.content_size = content.size();
    
    // Calculate actual hash
    auto actual_hash = crypto::Blake3::hash(content);
    result.actual_hash = actual_hash;
    
    // Compare
    result.is_valid = (actual_hash == expected_hash);
    
    if (!result.is_valid) {
        result.error_message = "Content hash mismatch - possible tampering detected";
        CASHEW_LOG_WARN("Content integrity check failed: expected {} != actual {}",
                       cashew::hash_to_hex(expected_hash), cashew::hash_to_hex(actual_hash));
    }
    
    return result;
}

ContentIntegrityChecker::VerificationResult ContentIntegrityChecker::verify_content_hash(
    const ContentHash& content_hash,
    const std::vector<uint8_t>& content
) {
    return verify_content(content, Hash256(content_hash.hash));
}

ContentIntegrityChecker::MerkleNode ContentIntegrityChecker::build_merkle_tree(
    const std::vector<uint8_t>& content,
    size_t chunk_size
) {
    std::vector<MerkleNode> leaf_nodes;
    
    // Build leaf nodes (one per chunk)
    for (size_t offset = 0; offset < content.size(); offset += chunk_size) {
        size_t length = std::min(chunk_size, content.size() - offset);
        
        std::vector<uint8_t> chunk(content.begin() + offset, 
                                   content.begin() + offset + length);
        
        MerkleNode leaf;
        leaf.hash = crypto::Blake3::hash(chunk);
        leaf.offset = offset;
        leaf.length = length;
        
        leaf_nodes.push_back(leaf);
    }
    
    // Build tree bottom-up
    std::vector<MerkleNode> current_level = leaf_nodes;
    
    while (current_level.size() > 1) {
        std::vector<MerkleNode> next_level;
        
        for (size_t i = 0; i < current_level.size(); i += 2) {
            MerkleNode parent;
            
            if (i + 1 < current_level.size()) {
                // Two children
                std::vector<uint8_t> combined;
                combined.insert(combined.end(), 
                              current_level[i].hash.begin(), 
                              current_level[i].hash.end());
                combined.insert(combined.end(), 
                              current_level[i + 1].hash.begin(), 
                              current_level[i + 1].hash.end());
                
                parent.hash = crypto::Blake3::hash(combined);
                parent.children.push_back(current_level[i].hash);
                parent.children.push_back(current_level[i + 1].hash);
            } else {
                // Single child (odd number)
                parent = current_level[i];
            }
            
            next_level.push_back(parent);
        }
        
        current_level = next_level;
    }
    
    return current_level.empty() ? MerkleNode{} : current_level[0];
}

bool ContentIntegrityChecker::verify_merkle_tree(
    const std::vector<uint8_t>& content,
    const MerkleNode& root,
    size_t chunk_size
) {
    auto calculated_root = build_merkle_tree(content, chunk_size);
    bool is_valid = (calculated_root.hash == root.hash);
    
    if (!is_valid) {
        CASHEW_LOG_WARN("Merkle tree verification failed");
    }
    
    return is_valid;
}

bool ContentIntegrityChecker::verify_chunk(
    const std::vector<uint8_t>& chunk,
    const Hash256& expected_chunk_hash
) {
    auto actual_hash = crypto::Blake3::hash(chunk);
    return (actual_hash == expected_chunk_hash);
}

ContentIntegrityChecker::IntegrityMetadata ContentIntegrityChecker::generate_metadata(
    const std::vector<uint8_t>& content,
    size_t chunk_size
) {
    IntegrityMetadata metadata;
    
    // Overall hash
    metadata.content_hash = crypto::Blake3::hash(content);
    metadata.content_size = content.size();
    
    // Merkle tree
    metadata.merkle_root = build_merkle_tree(content, chunk_size);
    
    // Individual chunk hashes
    for (size_t offset = 0; offset < content.size(); offset += chunk_size) {
        size_t length = std::min(chunk_size, content.size() - offset);
        std::vector<uint8_t> chunk(content.begin() + offset,
                                   content.begin() + offset + length);
        metadata.chunk_hashes.push_back(crypto::Blake3::hash(chunk));
    }
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    metadata.created_at = std::chrono::system_clock::to_time_t(now);
    
    CASHEW_LOG_DEBUG("Generated integrity metadata: {} bytes, {} chunks",
                    content.size(), metadata.chunk_hashes.size());
    
    return metadata;
}

bool ContentIntegrityChecker::verify_metadata(
    const std::vector<uint8_t>& content,
    const IntegrityMetadata& metadata
) {
    // Verify size
    if (content.size() != metadata.content_size) {
        CASHEW_LOG_WARN("Content size mismatch: {} != {}", 
                       content.size(), metadata.content_size);
        return false;
    }
    
    // Verify overall hash
    auto actual_hash = crypto::Blake3::hash(content);
    if (actual_hash != metadata.content_hash) {
        CASHEW_LOG_WARN("Content hash mismatch");
        return false;
    }
    
    // Verify Merkle tree
    if (!verify_merkle_tree(content, metadata.merkle_root)) {
        return false;
    }
    
    CASHEW_LOG_DEBUG("Content integrity verified successfully");
    return true;
}

} // namespace cashew::security
