#include "core/ledger/ledger.hpp"
#include "core/ledger/state.hpp"
#include "network/ledger_sync.hpp"
#include "network/state_reconciliation.hpp"
#include "core/postake/postake.hpp"
#include "core/reputation/reputation.hpp"
#include "core/reputation/attestation.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::core;
using namespace cashew::network;
using namespace cashew::reputation;

class LedgerReputationTest : public ::testing::Test {
protected:
    NodeIdentity node1, node2, node3;
    
    void SetUp() override {
        node1 = NodeIdentity::generate();
        node2 = NodeIdentity::generate();
        node3 = NodeIdentity::generate();
    }
};

// ============================================================================
// Ledger Tests
// ============================================================================

TEST_F(LedgerReputationTest, LedgerCreation) {
    Ledger ledger;
    
    EXPECT_EQ(ledger.height(), 0);
    EXPECT_TRUE(ledger.is_empty());
}

TEST_F(LedgerReputationTest, AppendEntry) {
    Ledger ledger;
    
    LedgerEntry entry{
        LedgerEntryType::KeyIssuance,
        node1.id(),
        {1, 2, 3},
        time::now()
    };
    
    bool success = ledger.append(entry);
    EXPECT_TRUE(success);
    EXPECT_EQ(ledger.height(), 1);
    EXPECT_FALSE(ledger.is_empty());
}

TEST_F(LedgerReputationTest, EventLog) {
    Ledger ledger;
    
    // Add multiple events
    ledger.append({LedgerEntryType::IdentityCreated, node1.id(), {}, time::now()});
    ledger.append({LedgerEntryType::KeyIssuance, node1.id(), {}, time::now()});
    ledger.append({LedgerEntryType::ThingPublished, node1.id(), {}, time::now()});
    
    EXPECT_EQ(ledger.height(), 3);
    
    // Query entries
    auto entries = ledger.get_entries(0, 3);
    EXPECT_EQ(entries.size(), 3);
}

TEST_F(LedgerReputationTest, StateQuery) {
    StateManager state;
    
    // Record state
    state.record_identity(node1.id());
    state.record_key_issuance(node1.id(), KeyType::Network, 1);
    
    // Query state
    EXPECT_TRUE(state.has_identity(node1.id()));
    EXPECT_EQ(state.get_key_count(node1.id()), 1);
}

TEST_F(LedgerReputationTest, ConflictDetection) {
    StateReconciliation reconciler;
    
    // Create conflicting states
    State state1, state2;
    state1.add_entry({LedgerEntryType::KeyIssuance, node1.id(), {1}, time::now()});
    state2.add_entry({LedgerEntryType::KeyIssuance, node1.id(), {2}, time::now()});
    
    auto conflicts = reconciler.detect_conflicts(state1, state2);
    EXPECT_GT(conflicts.size(), 0);
}

TEST_F(LedgerReputationTest, StateReconciliation) {
    StateReconciliation reconciler;
    
    State state1, state2;
    
    // Add different entries
    state1.add_entry({LedgerEntryType::KeyIssuance, node1.id(), {1}, time::now()});
    state2.add_entry({LedgerEntryType::KeyIssuance, node2.id(), {2}, time::now()});
    
    // Reconcile
    auto merged = reconciler.reconcile(state1, state2);
    
    // Merged state should have both entries
    EXPECT_GE(merged.entry_count(), 2);
}

TEST_F(LedgerReputationTest, LedgerSync) {
    Ledger ledger1, ledger2;
    
    // Add entries to ledger1
    ledger1.append({LedgerEntryType::IdentityCreated, node1.id(), {}, time::now()});
    ledger1.append({LedgerEntryType::KeyIssuance, node1.id(), {}, time::now()});
    
    LedgerSyncManager sync;
    
    // Sync ledger2 with ledger1
    sync.sync(ledger2, ledger1);
    
    EXPECT_EQ(ledger2.height(), ledger1.height());
}

// ============================================================================
// PoStake Tests
// ============================================================================

TEST_F(LedgerReputationTest, ContributionTracking) {
    ContributionTracker tracker;
    
    // Record contributions
    tracker.record_uptime(node1.id(), std::chrono::hours(24));
    tracker.record_bandwidth(node1.id(), 1024 * 1024 * 100); // 100MB
    tracker.record_storage(node1.id(), 1024 * 1024 * 1024); // 1GB
    
    auto score = tracker.calculate_score(node1.id());
    EXPECT_GT(score, 0.0);
}

TEST_F(LedgerReputationTest, PoStakeRewardDistribution) {
    PoStakeManager manager;
    
    // Set contributions
    manager.record_contribution(node1.id(), ContributionType::Uptime, 100.0);
    manager.record_contribution(node2.id(), ContributionType::Uptime, 50.0);
    
    // Calculate rewards
    auto reward1 = manager.calculate_reward(node1.id(), 1);
    auto reward2 = manager.calculate_reward(node2.id(), 1);
    
    // Node1 should get more reward (more contribution)
    EXPECT_GT(reward1, reward2);
}

TEST_F(LedgerReputationTest, HybridPoWPoStake) {
    HybridCoordinator coordinator;
    
    // Node1 completes PoW
    coordinator.record_pow_completion(node1.id(), 1);
    
    // Node1 also has PoStake contribution
    coordinator.record_postake_contribution(node1.id(), 100.0);
    
    // Calculate combined reward
    auto reward = coordinator.calculate_reward(node1.id(), 1);
    
    EXPECT_GT(reward.key_issuance, 0);
    EXPECT_GT(reward.reputation_boost, 0.0);
}

// ============================================================================
// Reputation Tests
// ============================================================================

TEST_F(LedgerReputationTest, ReputationCalculation) {
    ReputationManager manager;
    
    // Initial reputation
    auto score1 = manager.get_reputation(node1.id());
    EXPECT_GE(score1, 0.0);
    EXPECT_LE(score1, 1.0);
    
    // Record positive behavior
    manager.record_successful_action(node1.id(), ActionType::ContentHosting);
    
    auto score2 = manager.get_reputation(node1.id());
    EXPECT_GE(score2, score1);
}

TEST_F(LedgerReputationTest, ReputationDecay) {
    ReputationManager manager;
    
    // Set high reputation
    manager.set_reputation(node1.id(), 0.9);
    
    // Apply decay
    manager.apply_decay(node1.id(), 0.1);
    
    auto new_score = manager.get_reputation(node1.id());
    EXPECT_LT(new_score, 0.9);
}

TEST_F(LedgerReputationTest, AttestationCreation) {
    AttestationSigner signer(node1);
    
    Attestation attestation{
        node1.id(),
        node2.id(),
        AttestationType::Reliability,
        8, // rating out of 10
        "Good peer",
        time::now(),
        {} // signature
    };
    
    auto signed_att = signer.sign_attestation(attestation);
    EXPECT_GT(signed_att.signature.size(), 0);
}

TEST_F(LedgerReputationTest, AttestationVerification) {
    AttestationSigner signer(node1);
    
    Attestation attestation{
        node1.id(),
        node2.id(),
        AttestationType::Reliability,
        8,
        "Good peer",
        time::now(),
        {}
    };
    
    auto signed_att = signer.sign_attestation(attestation);
    
    // Verify signature
    EXPECT_TRUE(signer.verify_attestation(signed_att));
    
    // Tamper with attestation
    signed_att.rating = 10;
    EXPECT_FALSE(signer.verify_attestation(signed_att));
}

TEST_F(LedgerReputationTest, TrustGraph) {
    TrustGraph graph;
    
    // Add trust relationships
    graph.add_trust(node1.id(), node2.id(), 0.8);
    graph.add_trust(node2.id(), node3.id(), 0.7);
    graph.add_trust(node1.id(), node3.id(), 0.6);
    
    // Find paths
    auto path = graph.find_path(node1.id(), node3.id());
    ASSERT_TRUE(path.has_value());
    EXPECT_GE(path->size(), 1);
}

TEST_F(LedgerReputationTest, TrustPathFinding) {
    TrustPathFinder finder;
    
    // Build trust network
    finder.add_edge(node1.id(), node2.id(), 0.8);
    finder.add_edge(node2.id(), node3.id(), 0.7);
    
    // Find path from node1 to node3
    auto paths = finder.find_paths(node1.id(), node3.id(), 3);
    EXPECT_GT(paths.size(), 0);
    
    // Calculate trust score along path
    if (!paths.empty()) {
        auto score = finder.calculate_path_trust(paths[0]);
        EXPECT_GT(score, 0.0);
        EXPECT_LE(score, 1.0);
    }
}

TEST_F(LedgerReputationTest, VouchingWorkflow) {
    VouchingWorkflow workflow;
    
    // Node1 requests vouch from Node2
    auto request = workflow.create_vouch_request(node1.id(), node2.id(), KeyType::Network);
    EXPECT_EQ(request.requester, node1.id());
    EXPECT_EQ(request.voucher, node2.id());
    
    // Node2 accepts
    auto vouch = workflow.accept_vouch_request(request, 0.8);
    EXPECT_EQ(vouch.strength, 0.8);
    
    // Record vouch
    workflow.record_vouch(vouch);
    
    // Query vouches
    auto vouches = workflow.get_vouches_for(node1.id());
    EXPECT_GE(vouches.size(), 1);
}

TEST_F(LedgerReputationTest, VouchRevocation) {
    VouchingWorkflow workflow;
    
    // Create and record vouch
    auto request = workflow.create_vouch_request(node1.id(), node2.id(), KeyType::Network);
    auto vouch = workflow.accept_vouch_request(request, 0.8);
    workflow.record_vouch(vouch);
    
    // Revoke vouch
    bool success = workflow.revoke_vouch(vouch.vouch_id, node2.id());
    EXPECT_TRUE(success);
    
    // Vouch should be marked as revoked
    auto vouches = workflow.get_active_vouches_for(node1.id());
    // Should not include revoked vouch
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
