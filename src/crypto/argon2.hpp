#pragma once

#include "cashew/common.hpp"
#include <cstdint>
#include <vector>

namespace cashew::crypto {

/**
 * Argon2id - Memory-hard password hashing and PoW
 * Used for adaptive Proof-of-Work puzzles
 */
class Argon2 {
public:
    struct Params {
        uint32_t memory_cost_kb;    // Memory usage in KB
        uint32_t time_cost;          // Number of iterations
        uint32_t parallelism;        // Number of parallel threads
        
        // Presets
        static Params interactive() {
            return {65536, 2, 1};    // 64 MB, 2 iterations, 1 thread
        }
        
        static Params moderate() {
            return {262144, 3, 1};   // 256 MB, 3 iterations, 1 thread
        }
        
        static Params sensitive() {
            return {1048576, 4, 1};  // 1 GB, 4 iterations, 1 thread
        }
    };
    
    /**
     * Hash a password with Argon2id
     * @param password Password/data to hash
     * @param salt Salt (16 bytes minimum)
     * @param params Argon2 parameters
     * @param output_len Output hash length (default 32 bytes)
     * @return Hash
     */
    static bytes hash(
        const bytes& password,
        const bytes& salt,
        const Params& params,
        size_t output_len = 32
    );
    
    /**
     * Verify a password against a hash
     * @param password Password to verify
     * @param salt Salt used in hashing
     * @param hash Expected hash
     * @param params Argon2 parameters used
     * @return True if password matches
     */
    static bool verify(
        const bytes& password,
        const bytes& salt,
        const bytes& hash,
        const Params& params
    );
    
    /**
     * Generate a PoW puzzle solution
     * @param challenge Challenge data
     * @param nonce Nonce to try
     * @param params Argon2 parameters
     * @return Hash result
     */
    static Hash256 solve_puzzle(
        const bytes& challenge,
        uint64_t nonce,
        const Params& params
    );
};

} // namespace cashew::crypto
