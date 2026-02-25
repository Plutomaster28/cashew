#include "core/pow/pow.hpp"
#include "crypto/argon2.hpp"
#include "crypto/blake3.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>
#include <chrono>

using namespace cashew;
using namespace cashew::core;
using namespace cashew::crypto;

class PoWTest : public ::testing::Test {
protected:
    bytes test_challenge = {'t', 'e', 's', 't', '_', 'c', 'h', 'a', 'l', 'l', 'e', 'n', 'g', 'e'};
    uint64_t test_epoch = 12345;
};

TEST_F(PoWTest, PuzzleGeneration) {
    // Generate puzzle with minimum difficulty
    uint32_t difficulty = ProofOfWork::MIN_DIFFICULTY;
    
    auto puzzle = ProofOfWork::generate_puzzle(test_challenge, test_epoch, difficulty);
    
    EXPECT_EQ(puzzle.challenge, test_challenge);
    EXPECT_EQ(puzzle.difficulty, difficulty);
    EXPECT_EQ(puzzle.epoch, test_epoch);
    EXPECT_GT(puzzle.timestamp, 0);
    EXPECT_GT(puzzle.params.memory_cost_kb, 0);
    EXPECT_GT(puzzle.params.time_cost, 0);
}

TEST_F(PoWTest, DifficultyBounds) {
    // Test minimum difficulty
    auto puzzle_min = ProofOfWork::generate_puzzle(test_challenge, test_epoch, ProofOfWork::MIN_DIFFICULTY);
    EXPECT_EQ(puzzle_min.difficulty, ProofOfWork::MIN_DIFFICULTY);
    
    // Test maximum difficulty
    auto puzzle_max = ProofOfWork::generate_puzzle(test_challenge, test_epoch, ProofOfWork::MAX_DIFFICULTY);
    EXPECT_EQ(puzzle_max.difficulty, ProofOfWork::MAX_DIFFICULTY);
    
    // Test that difficulty is clamped
    auto puzzle_high = ProofOfWork::generate_puzzle(test_challenge, test_epoch, ProofOfWork::MAX_DIFFICULTY + 10);
    EXPECT_LE(puzzle_high.difficulty, ProofOfWork::MAX_DIFFICULTY);
}

TEST_F(PoWTest, PuzzleSolvingLowDifficulty) {
    // Create easy puzzle (should solve quickly)
    uint32_t easy_difficulty = ProofOfWork::MIN_DIFFICULTY;
    auto puzzle = ProofOfWork::generate_puzzle(test_challenge, test_epoch, easy_difficulty);
    
    // Solve with limited attempts
    auto solution = ProofOfWork::solve_puzzle(puzzle, 10000);
    
    ASSERT_TRUE(solution.has_value());
    EXPECT_EQ(solution->difficulty, easy_difficulty);
    EXPECT_GT(solution->nonce, 0);
    EXPECT_EQ(solution->solution_hash.size(), 32);
    EXPECT_GT(solution->compute_time_ms, 0);
}

TEST_F(PoWTest, SolutionVerification) {
    // Generate and solve puzzle
    uint32_t difficulty = ProofOfWork::MIN_DIFFICULTY;
    auto puzzle = ProofOfWork::generate_puzzle(test_challenge, test_epoch, difficulty);
    auto solution = ProofOfWork::solve_puzzle(puzzle, 100000);
    
    ASSERT_TRUE(solution.has_value());
    
    // Verify valid solution
    bool valid = ProofOfWork::verify_solution(puzzle, solution.value());
    EXPECT_TRUE(valid);
}

TEST_F(PoWTest, InvalidSolutionRejection) {
    // Generate puzzle
    uint32_t difficulty = ProofOfWork::MIN_DIFFICULTY;
    auto puzzle = ProofOfWork::generate_puzzle(test_challenge, test_epoch, difficulty);
    
    // Create invalid solution (wrong nonce)
    PowSolution fake_solution;
    fake_solution.solution_hash = Hash256{};
    fake_solution.nonce = 12345;
    fake_solution.difficulty = difficulty;
    fake_solution.compute_time_ms = 100;
    
    // Should fail verification
    bool valid = ProofOfWork::verify_solution(puzzle, fake_solution);
    EXPECT_FALSE(valid);
}

TEST_F(PoWTest, DifficultyAdjustmentIncrease) {
    // Solve time too fast (less than half target) -> increase difficulty
    uint64_t very_fast_solve_time = ProofOfWork::TARGET_SOLVE_TIME_MS / 3;  // 200 seconds (< 5 min)
    uint32_t current_difficulty = 10;
    
    uint32_t new_difficulty = ProofOfWork::adjust_difficulty(very_fast_solve_time, current_difficulty);
    
    // Difficulty should increase
    EXPECT_GT(new_difficulty, current_difficulty);
    EXPECT_LE(new_difficulty, ProofOfWork::MAX_DIFFICULTY);
}

TEST_F(PoWTest, DifficultyAdjustmentDecrease) {
    // Solve time too slow (more than double target) -> decrease difficulty
    uint64_t very_slow_solve_time = ProofOfWork::TARGET_SOLVE_TIME_MS * 3;  // 30 minutes (> 20 min)
    uint32_t current_difficulty = 10;
    
    uint32_t new_difficulty = ProofOfWork::adjust_difficulty(very_slow_solve_time, current_difficulty);
    
    // Difficulty should decrease
    EXPECT_LT(new_difficulty, current_difficulty);
    EXPECT_GE(new_difficulty, ProofOfWork::MIN_DIFFICULTY);
}

TEST_F(PoWTest, DifficultyAdjustmentStable) {
    // Solve time at target -> difficulty stable
    uint64_t target_solve_time = ProofOfWork::TARGET_SOLVE_TIME_MS;
    uint32_t current_difficulty = 10;
    
    uint32_t new_difficulty = ProofOfWork::adjust_difficulty(target_solve_time, current_difficulty);
    
    // Difficulty should be close to current (within small margin)
    EXPECT_GE(new_difficulty, current_difficulty - 1);
    EXPECT_LE(new_difficulty, current_difficulty + 1);
}

TEST_F(PoWTest, NodeBenchmarking) {
    // Run short benchmark
    uint64_t benchmark_duration_ms = 100;  // Very short for testing
    
    uint64_t hashes_per_second = ProofOfWork::benchmark_node(benchmark_duration_ms);
    
    // Should get some hash rate
    EXPECT_GT(hashes_per_second, 0);
}

TEST_F(PoWTest, StartingDifficultyFromBenchmark) {
    // Simulate slow hardware
    uint64_t slow_hps = 100;
    uint32_t slow_difficulty = ProofOfWork::get_starting_difficulty(slow_hps);
    EXPECT_GE(slow_difficulty, ProofOfWork::MIN_DIFFICULTY);
    
    // Simulate fast hardware
    uint64_t fast_hps = 10000;
    uint32_t fast_difficulty = ProofOfWork::get_starting_difficulty(fast_hps);
    EXPECT_GE(fast_difficulty, ProofOfWork::MIN_DIFFICULTY);
    
    // Faster hardware should get higher difficulty
    EXPECT_GE(fast_difficulty, slow_difficulty);
}

TEST_F(PoWTest, SolveWithMaxAttempts) {
    // Create puzzle and limit attempts
    uint32_t difficulty = ProofOfWork::MIN_DIFFICULTY + 2;  // Slightly harder
    auto puzzle = ProofOfWork::generate_puzzle(test_challenge, test_epoch, difficulty);
    
    // Try with very few attempts (might not find solution)
    uint64_t max_attempts = 10;
    auto solution = ProofOfWork::solve_puzzle(puzzle, max_attempts);
    
    // Either finds solution or gives up
    if (solution.has_value()) {
        // If found, should be valid
        EXPECT_TRUE(ProofOfWork::verify_solution(puzzle, solution.value()));
    }
    // If not found, that's also valid behavior with low max_attempts
}

TEST_F(PoWTest, DifferentEpochsDifferentPuzzles) {
    uint32_t difficulty = ProofOfWork::MIN_DIFFICULTY;
    
    auto puzzle1 = ProofOfWork::generate_puzzle(test_challenge, 100, difficulty);
    auto puzzle2 = ProofOfWork::generate_puzzle(test_challenge, 101, difficulty);
    
    // Different epochs should produce different puzzles
    EXPECT_NE(puzzle1.epoch, puzzle2.epoch);
}

TEST_F(PoWTest, DifferentChallengesDifferentPuzzles) {
    bytes challenge1 = {'a', 'b', 'c'};
    bytes challenge2 = {'x', 'y', 'z'};
    uint32_t difficulty = ProofOfWork::MIN_DIFFICULTY;
    
    auto puzzle1 = ProofOfWork::generate_puzzle(challenge1, test_epoch, difficulty);
    auto puzzle2 = ProofOfWork::generate_puzzle(challenge2, test_epoch, difficulty);
    
    // Different challenges should produce different puzzles
    EXPECT_NE(puzzle1.challenge, puzzle2.challenge);
}

TEST_F(PoWTest, TargetSolveTimeConstant) {
    // Verify the target is 10 minutes
    EXPECT_EQ(ProofOfWork::TARGET_SOLVE_TIME_MS, 10 * 60 * 1000);
}

TEST_F(PoWTest, DifficultyRangeConstants) {
    // Verify difficulty bounds
    EXPECT_GT(ProofOfWork::MIN_DIFFICULTY, 0);
    EXPECT_GT(ProofOfWork::MAX_DIFFICULTY, ProofOfWork::MIN_DIFFICULTY);
    EXPECT_LE(ProofOfWork::MAX_DIFFICULTY, 32);  // Hash is 32 bytes = 256 bits
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
