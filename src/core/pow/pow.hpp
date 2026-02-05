#pragma once

#include "cashew/common.hpp"
#include "crypto/argon2.hpp"
#include <cstdint>
#include <optional>

namespace cashew::core {

/**
 * Proof-of-Work puzzle and solution
 */
struct PowPuzzle {
    bytes challenge;              // Challenge data (network entropy)
    uint32_t difficulty;          // Target difficulty (leading zero bits)
    crypto::Argon2::Params params; // Argon2 parameters
    uint64_t epoch;               // Epoch number
    uint64_t timestamp;           // When puzzle was created
};

struct PowSolution {
    Hash256 solution_hash;        // Result hash
    uint64_t nonce;               // Nonce that produces solution
    uint32_t difficulty;          // Difficulty of puzzle
    uint64_t compute_time_ms;     // Time taken to solve (for benchmarking)
};

/**
 * Proof-of-Work system with adaptive difficulty
 * 
 * Uses Argon2id for memory-hard puzzles that are fair across different hardware.
 * Difficulty adapts per-node based on their capability.
 */
class ProofOfWork {
public:
    /**
     * Target time per puzzle (10 minutes)
     */
    static constexpr uint64_t TARGET_SOLVE_TIME_MS = 10 * 60 * 1000;
    
    /**
     * Minimum difficulty (leading zero bits)
     */
    static constexpr uint32_t MIN_DIFFICULTY = 4;
    
    /**
     * Maximum difficulty (leading zero bits)
     */
    static constexpr uint32_t MAX_DIFFICULTY = 32;
    
    /**
     * Generate a new puzzle for current epoch
     * @param challenge_data Network entropy/challenge
     * @param epoch Current epoch
     * @param node_difficulty Difficulty for this specific node
     * @return Puzzle
     */
    static PowPuzzle generate_puzzle(
        const bytes& challenge_data,
        uint64_t epoch,
        uint32_t node_difficulty
    );
    
    /**
     * Solve a puzzle (blocking, can take minutes)
     * @param puzzle Puzzle to solve
     * @param max_attempts Maximum nonce attempts (0 = unlimited)
     * @return Solution or nullopt if max_attempts reached
     */
    static std::optional<PowSolution> solve_puzzle(
        const PowPuzzle& puzzle,
        uint64_t max_attempts = 0
    );
    
    /**
     * Verify a puzzle solution
     * @param puzzle Original puzzle
     * @param solution Claimed solution
     * @return True if valid
     */
    static bool verify_solution(
        const PowPuzzle& puzzle,
        const PowSolution& solution
    );
    
    /**
     * Calculate adaptive difficulty for a node
     * @param previous_solve_time_ms Previous solve time
     * @param current_difficulty Current difficulty
     * @return Adjusted difficulty
     */
    static uint32_t adjust_difficulty(
        uint64_t previous_solve_time_ms,
        uint32_t current_difficulty
    );
    
    /**
     * Benchmark node capability
     * @param test_duration_ms Duration to run benchmark
     * @return Estimated hashes per second
     */
    static uint64_t benchmark_node(uint64_t test_duration_ms = 10000);
    
    /**
     * Get recommended starting difficulty based on benchmark
     * @param hashes_per_second Node capability
     * @return Recommended difficulty
     */
    static uint32_t get_starting_difficulty(uint64_t hashes_per_second);

private:
    /**
     * Count leading zero bits in hash
     */
    static uint32_t count_leading_zeros(const Hash256& hash);
    
    /**
     * Check if hash meets difficulty requirement
     */
    static bool meets_difficulty(const Hash256& hash, uint32_t difficulty);
};

} // namespace cashew::core
