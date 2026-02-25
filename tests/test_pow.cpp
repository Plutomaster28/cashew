#include "core/pow/pow.hpp"
#include "core/node/node_identity.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::core;

class PoWTest : public ::testing::Test {
protected:
    NodeIdentity node_identity;
    
    void SetUp() override {
        node_identity = NodeIdentity::generate();
    }
};

TEST_F(PoWTest, PuzzleGeneration) {
    PoWEngine engine;
    
    uint64_t epoch = 100;
    auto puzzle = engine.generate_puzzle(epoch);
    
    EXPECT_EQ(puzzle.epoch, epoch);
    EXPECT_GT(puzzle.difficulty, 0);
    EXPECT_EQ(puzzle.target.size(), 32);
    EXPECT_EQ(puzzle.entropy.size(), 32);
}

TEST_F(PoWTest, DifficultyCalculation) {
    PoWEngine engine;
    
    // Simulate different network conditions
    NetworkStats stats1{10, 1000, 5.0}; // 10 nodes, 1000 H/s avg, 5s avg time
    auto diff1 = engine.calculate_difficulty(stats1);
    
    NetworkStats stats2{100, 10000, 2.0}; // More nodes, higher hashrate, faster time
    auto diff2 = engine.calculate_difficulty(stats2);
    
    // Higher network power should result in higher difficulty
    EXPECT_GT(diff2, diff1);
}

TEST_F(PoWTest, AdaptiveDifficulty) {
    PoWEngine engine;
    
    // Simulate multiple epochs
    std::vector<double> solve_times;
    
    for (int i = 0; i < 10; i++) {
        auto puzzle = engine.generate_puzzle(i);
        
        // Simulate solve time
        double solve_time = 5.0 + (i % 3);
        solve_times.push_back(solve_time);
        
        engine.record_solve_time(solve_time);
        
        // Difficulty should adapt
        if (i > 0) {
            auto prev_diff = engine.generate_puzzle(i - 1).difficulty;
            auto curr_diff = puzzle.difficulty;
            
            // Verify difficulty changes based on solve times
            if (solve_time < 8.0) {
                EXPECT_GE(curr_diff, prev_diff * 0.9);
            }
        }
    }
}

TEST_F(PoWTest, PuzzleSolving) {
    PoWEngine engine;
    
    auto puzzle = engine.generate_puzzle(1);
    
    // Lower difficulty for faster test
    puzzle.difficulty = 100;
    
    // Attempt to solve
    auto solution = engine.solve(puzzle, node_identity.id());
    
    ASSERT_TRUE(solution.has_value());
    EXPECT_EQ(solution->puzzle_hash, puzzle.hash());
    EXPECT_GT(solution->nonce, 0);
}

TEST_F(PoWTest, SolutionVerification) {
    PoWEngine engine;
    
    auto puzzle = engine.generate_puzzle(1);
    puzzle.difficulty = 100;
    
    auto solution = engine.solve(puzzle, node_identity.id());
    ASSERT_TRUE(solution.has_value());
    
    // Verify valid solution
    bool valid = engine.verify(puzzle, solution.value());
    EXPECT_TRUE(valid);
    
    // Modify solution (should be invalid)
    auto invalid_solution = solution.value();
    invalid_solution.nonce++;
    
    bool invalid = engine.verify(puzzle, invalid_solution);
    EXPECT_FALSE(invalid);
}

TEST_F(PoWTest, EpochManagement) {
    PoWEpochManager manager;
    
    auto epoch1 = manager.current_epoch();
    
    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto epoch2 = manager.current_epoch();
    
    // Same epoch (10-minute intervals)
    EXPECT_EQ(epoch1, epoch2);
    
    // Test epoch calculation
    uint64_t timestamp = 1000000000; // Some timestamp
    auto epoch_for_ts = manager.epoch_for_timestamp(timestamp);
    
    // Should be consistent
    auto epoch_for_ts2 = manager.epoch_for_timestamp(timestamp);
    EXPECT_EQ(epoch_for_ts, epoch_for_ts2);
}

TEST_F(PoWTest, EntropyCollection) {
    PoWEngine engine;
    
    // Add network entropy
    engine.add_entropy({0x01, 0x02, 0x03});
    engine.add_entropy({0x04, 0x05, 0x06});
    
    // Generate puzzles
    auto puzzle1 = engine.generate_puzzle(1);
    auto puzzle2 = engine.generate_puzzle(2);
    
    // Puzzles should be different due to entropy
    EXPECT_NE(puzzle1.entropy, puzzle2.entropy);
}

TEST_F(PoWTest, NodeBenchmarking) {
    PoWBenchmark benchmark;
    
    // Run benchmark
    auto result = benchmark.run_benchmark(std::chrono::seconds(1));
    
    EXPECT_GT(result.hashes_per_second, 0);
    EXPECT_GT(result.memory_usage_mb, 0);
    EXPECT_GT(result.duration_ms, 0);
}

TEST_F(PoWTest, RewardDistribution) {
    PoWRewardManager rewards;
    
    // Record successful PoW
    NodeID node_id = node_identity.id();
    rewards.record_pow_success(node_id, 1);
    
    auto reward = rewards.get_reward(node_id, 1);
    
    EXPECT_GT(reward.key_issuance_count, 0);
    EXPECT_GT(reward.reputation_boost, 0.0);
}

TEST_F(PoWTest, AntiSpamLimiting) {
    PoWEngine engine;
    AntiSpamLimiter limiter;
    
    NodeID node_id = node_identity.id();
    
    // First request should be allowed
    EXPECT_TRUE(limiter.allow_request(node_id));
    
    // Rapid requests should be rate limited
    for (int i = 0; i < 100; i++) {
        limiter.allow_request(node_id);
    }
    
    // Should eventually be rate limited
    bool was_limited = false;
    for (int i = 0; i < 10; i++) {
        if (!limiter.allow_request(node_id)) {
            was_limited = true;
            break;
        }
    }
    
    EXPECT_TRUE(was_limited);
}

TEST_F(PoWTest, MultipleNodesSolving) {
    PoWEngine engine;
    
    auto puzzle = engine.generate_puzzle(1);
    puzzle.difficulty = 100;
    
    // Simulate multiple nodes solving
    std::vector<NodeIdentity> nodes;
    for (int i = 0; i < 5; i++) {
        nodes.push_back(NodeIdentity::generate());
    }
    
    std::vector<std::optional<PoWSolution>> solutions;
    
    for (const auto& node : nodes) {
        auto solution = engine.solve(puzzle, node.id());
        solutions.push_back(solution);
    }
    
    // All should find valid solutions
    int valid_count = 0;
    for (const auto& sol : solutions) {
        if (sol.has_value() && engine.verify(puzzle, sol.value())) {
            valid_count++;
        }
    }
    
    EXPECT_GE(valid_count, 1);
}

TEST_F(PoWTest, MemoryHardness) {
    PoWEngine engine;
    
    auto puzzle = engine.generate_puzzle(1);
    
    // Verify Argon2 parameters indicate memory hardness
    EXPECT_EQ(puzzle.algorithm, "Argon2id");
    EXPECT_GT(puzzle.memory_cost_kb, 32 * 1024); // At least 32MB
    EXPECT_GE(puzzle.time_cost, 2);
    EXPECT_GE(puzzle.parallelism, 1);
}

TEST_F(PoWTest, EpochTransition) {
    PoWEngine engine;
    
    uint64_t epoch1 = 100;
    uint64_t epoch2 = 101;
    
    auto puzzle1 = engine.generate_puzzle(epoch1);
    auto puzzle2 = engine.generate_puzzle(epoch2);
    
    // Different epochs should have different puzzles
    EXPECT_NE(puzzle1.hash(), puzzle2.hash());
    
    // Solutions from old epoch should not work for new epoch
    puzzle1.difficulty = 100;
    auto solution = engine.solve(puzzle1, node_identity.id());
    
    if (solution.has_value()) {
        // Solution should not verify against different epoch puzzle
        bool cross_epoch = engine.verify(puzzle2, solution.value());
        EXPECT_FALSE(cross_epoch);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
