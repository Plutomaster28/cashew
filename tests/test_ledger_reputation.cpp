#include "core/ledger/ledger.hpp"
#include "core/ledger/state.hpp"
#include "core/reputation/reputation.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::ledger;
using namespace cashew::reputation;

namespace {

NodeID make_node(uint8_t seed) {
    Hash256 id{};
    id[0] = seed;
    id[31] = static_cast<uint8_t>(seed ^ 0xA5);
    return NodeID(id);
}

Hash256 make_hash(uint8_t seed) {
    Hash256 h{};
    h[0] = seed;
    h[31] = static_cast<uint8_t>(seed ^ 0x5A);
    return h;
}

} // namespace

TEST(LedgerReputationTest, LedgerEventRecordingAndChainValidation) {
    const NodeID local = make_node(1);
    Ledger ledger(local);

    ledger.record_node_joined(local);
    ledger.record_key_issued(core::KeyType::NETWORK, 2, IssuanceMethod::POW, make_hash(3));
    ledger.record_network_created(make_hash(9));

    EXPECT_EQ(ledger.event_count(), 3u);
    EXPECT_TRUE(ledger.validate_chain());

    const auto key_events = ledger.get_events_by_type(EventType::KEY_ISSUED);
    EXPECT_EQ(key_events.size(), 1u);
}

TEST(LedgerReputationTest, StateManagerBuildsCapabilitiesFromLedger) {
    const NodeID local = make_node(2);
    const Hash256 network_id = make_hash(12);

    Ledger ledger(local);
    ledger.record_node_joined(local);
    ledger.record_key_issued(core::KeyType::SERVICE, 1, IssuanceMethod::POSTAKE, make_hash(13));
    ledger.record_reputation_update(local, 75, "bootstrap");
    ledger.record_network_created(network_id);
    ledger.record_network_member_added(network_id, local, "FOUNDER");

    StateManager state(ledger);

    EXPECT_TRUE(state.is_node_active(local));
    EXPECT_EQ(state.get_node_key_balance(local, core::KeyType::SERVICE), 1u);
    EXPECT_EQ(state.get_node_reputation(local), 75);
    EXPECT_TRUE(state.is_node_in_network(local, network_id));
    EXPECT_TRUE(state.can_node_host_things(local));
}

TEST(LedgerReputationTest, TrustGraphSupportsTransitiveTrust) {
    const NodeID a = make_node(10);
    const NodeID b = make_node(11);
    const NodeID c = make_node(12);

    TrustGraph graph;
    graph.add_edge(a, b, 0.8f);
    graph.add_edge(b, c, 0.5f);

    const float transitive = graph.calculate_transitive_trust(a, c, 3);
    EXPECT_GT(transitive, 0.39f);
    EXPECT_LT(transitive, 0.41f);

    const auto community = graph.find_trust_community(a, 0.4f);
    EXPECT_TRUE(community.find(a) != community.end());
    EXPECT_TRUE(community.find(b) != community.end());
}

TEST(LedgerReputationTest, ReputationManagerTracksActionsAndScores) {
    const NodeID local = make_node(21);

    Ledger ledger(local);
    ledger.record_node_joined(local);
    StateManager state(ledger);
    ReputationManager reputation(state);

    EXPECT_EQ(reputation.get_reputation(local), 0);

    reputation.record_action(local, ReputationAction::HOST_THING);
    const int32_t after_hosting = reputation.get_reputation(local);
    EXPECT_GT(after_hosting, 0);

    reputation.apply_score_delta(local, 25, "extra-credit");
    EXPECT_EQ(reputation.get_reputation(local), after_hosting + 25);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
