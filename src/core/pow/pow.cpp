#include "pow.hpp"
#include "utils/logger.hpp"
#include "crypto/random.hpp"
#include <chrono>
#include <cstring>

namespace cashew::core {

PowPuzzle ProofOfWork::generate_puzzle(
    const bytes& challenge_data,
    uint64_t epoch,
    uint32_t node_difficulty
) {
    // Clamp difficulty to valid range
    uint32_t difficulty = std::max(MIN_DIFFICULTY, 
                                   std::min(node_difficulty, MAX_DIFFICULTY));
    
    // Generate puzzle
    PowPuzzle puzzle;
    puzzle.challenge = challenge_data;
    puzzle.difficulty = difficulty;
    puzzle.epoch = epoch;
    puzzle.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Set Argon2 params based on difficulty
    if (difficulty <= 8) {
        puzzle.params = crypto::Argon2::Params::interactive();
    } else if (difficulty <= 16) {
        puzzle.params = crypto::Argon2::Params::moderate();
    } else {
        puzzle.params = crypto::Argon2::Params::sensitive();
    }
    
    CASHEW_LOG_INFO("Generated PoW puzzle: epoch={}, difficulty={}, mem={}KB", 
                   epoch, difficulty, puzzle.params.memory_cost_kb);
    
    return puzzle;
}

std::optional<PowSolution> ProofOfWork::solve_puzzle(
    const PowPuzzle& puzzle,
    uint64_t max_attempts
) {
    CASHEW_LOG_INFO("Solving PoW puzzle (difficulty={})", puzzle.difficulty);
    
    auto start_time = std::chrono::steady_clock::now();
    uint64_t attempt = 0;
    
    while (max_attempts == 0 || attempt < max_attempts) {
        uint64_t nonce = crypto::Random::generate_uint64();
        
        // Compute hash with Argon2
        Hash256 hash = crypto::Argon2::solve_puzzle(
            puzzle.challenge,
            nonce,
            puzzle.params
        );
        
        // Check if solution meets difficulty
        if (meets_difficulty(hash, puzzle.difficulty)) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time
            );
            
            PowSolution solution;
            solution.solution_hash = hash;
            solution.nonce = nonce;
            solution.difficulty = puzzle.difficulty;
            solution.compute_time_ms = duration.count();
            
            CASHEW_LOG_INFO("PoW solved! attempts={}, time={}ms", 
                          attempt + 1, solution.compute_time_ms);
            
            return solution;
        }
        
        ++attempt;
        
        // Log progress every 10 attempts
        if (attempt % 10 == 0) {
            CASHEW_LOG_DEBUG("PoW progress: {} attempts", attempt);
        }
    }
    
    CASHEW_LOG_WARN("PoW failed: max attempts {} reached", max_attempts);
    return std::nullopt;
}

bool ProofOfWork::verify_solution(
    const PowPuzzle& puzzle,
    const PowSolution& solution
) {
    // Verify difficulty matches
    if (solution.difficulty != puzzle.difficulty) {
        CASHEW_LOG_ERROR("PoW verification failed: difficulty mismatch");
        return false;
    }
    
    // Recompute hash with claimed nonce
    Hash256 computed_hash = crypto::Argon2::solve_puzzle(
        puzzle.challenge,
        solution.nonce,
        puzzle.params
    );
    
    // Verify hash matches
    if (computed_hash != solution.solution_hash) {
        CASHEW_LOG_ERROR("PoW verification failed: hash mismatch");
        return false;
    }
    
    // Verify difficulty requirement
    if (!meets_difficulty(computed_hash, puzzle.difficulty)) {
        CASHEW_LOG_ERROR("PoW verification failed: insufficient difficulty");
        return false;
    }
    
    CASHEW_LOG_DEBUG("PoW solution verified");
    return true;
}

uint32_t ProofOfWork::adjust_difficulty(
    uint64_t previous_solve_time_ms,
    uint32_t current_difficulty
) {
    // If solved too fast, increase difficulty
    if (previous_solve_time_ms < TARGET_SOLVE_TIME_MS / 2) {
        uint32_t new_difficulty = current_difficulty + 1;
        CASHEW_LOG_INFO("Adjusting difficulty: {} -> {} (solved too fast)", 
                       current_difficulty, new_difficulty);
        return std::min(new_difficulty, MAX_DIFFICULTY);
    }
    
    // If solved too slow, decrease difficulty
    if (previous_solve_time_ms > TARGET_SOLVE_TIME_MS * 2) {
        uint32_t new_difficulty = current_difficulty > MIN_DIFFICULTY 
                                 ? current_difficulty - 1 
                                 : MIN_DIFFICULTY;
        CASHEW_LOG_INFO("Adjusting difficulty: {} -> {} (solved too slow)", 
                       current_difficulty, new_difficulty);
        return new_difficulty;
    }
    
    // Within acceptable range, keep same difficulty
    return current_difficulty;
}

uint64_t ProofOfWork::benchmark_node(uint64_t test_duration_ms) {
    CASHEW_LOG_INFO("Benchmarking node capability...");
    
    bytes test_data = crypto::Random::generate(32);
    auto params = crypto::Argon2::Params::interactive();
    
    auto start = std::chrono::steady_clock::now();
    uint64_t hashes = 0;
    
    while (true) {
        crypto::Argon2::solve_puzzle(test_data, hashes, params);
        ++hashes;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start
        ).count();
        
        if (static_cast<uint64_t>(elapsed) >= test_duration_ms) {
            break;
        }
    }
    
    uint64_t hashes_per_second = (hashes * 1000) / test_duration_ms;
    CASHEW_LOG_INFO("Benchmark complete: {} hashes/sec", hashes_per_second);
    
    return hashes_per_second;
}

uint32_t ProofOfWork::get_starting_difficulty(uint64_t hashes_per_second) {
    // Simple heuristic: more capable nodes get higher starting difficulty
    if (hashes_per_second < 10) {
        return MIN_DIFFICULTY;
    } else if (hashes_per_second < 50) {
        return MIN_DIFFICULTY + 2;
    } else if (hashes_per_second < 100) {
        return MIN_DIFFICULTY + 4;
    } else {
        return MIN_DIFFICULTY + 6;
    }
}

uint32_t ProofOfWork::count_leading_zeros(const Hash256& hash) {
    uint32_t count = 0;
    
    for (byte b : hash) {
        if (b == 0) {
            count += 8;
        } else {
            // Count leading zeros in this byte
            byte mask = 0x80;
            while ((b & mask) == 0 && mask != 0) {
                ++count;
                mask >>= 1;
            }
            break;
        }
    }
    
    return count;
}

bool ProofOfWork::meets_difficulty(const Hash256& hash, uint32_t difficulty) {
    return count_leading_zeros(hash) >= difficulty;
}

} // namespace cashew::core
