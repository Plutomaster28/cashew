#include "security/access.hpp"
#include "security/content_integrity.hpp"
#include "security/attack_prevention.hpp"
#include "core/ledger/ledger.hpp"
#include "core/ledger/state.hpp"
#include "cashew/common.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::security;
using namespace cashew::ledger;

namespace {

NodeID make_node(uint8_t seed) {
    Hash256 id{};
    id[0] = seed;
    id[31] = static_cast<uint8_t>(seed ^ 0x5C);
    return NodeID(id);
}

Hash256 make_hash(uint8_t seed) {
    Hash256 h{};
    h[0] = seed;
    h[31] = static_cast<uint8_t>(seed ^ 0xA3);
    return h;
}

} // namespace

TEST(SecurityTest, ContentIntegrityPassesAndDetectsTamper) {
    std::vector<uint8_t> data = {'c', 'a', 's', 'h', 'e', 'w'};
    const Hash256 expected = crypto::Blake3::hash(data);

    const auto ok = ContentIntegrityChecker::verify_content(data, expected);
    EXPECT_TRUE(ok.is_valid);

    data[0] = 'C';
    const auto bad = ContentIntegrityChecker::verify_content(data, expected);
    EXPECT_FALSE(bad.is_valid);
}

TEST(SecurityTest, RateLimiterEnforcesBurstAndMinuteLimits) {
    RateLimitPolicy policy;
    policy.max_requests_per_minute = 2;
    policy.max_requests_per_hour = 5;
    policy.burst_size = 2;

    RateLimiter limiter(policy);
    const NodeID id = make_node(1);

    EXPECT_TRUE(limiter.allow_request(id));
    EXPECT_TRUE(limiter.allow_request(id));
    EXPECT_FALSE(limiter.allow_request(id));
}

TEST(SecurityTest, DDoSMitigationBlocksFloodingIp) {
    DDoSMitigation mitigation;
    const std::string ip = "192.168.0.42";

    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(mitigation.allow_connection(ip));
        mitigation.record_connection(ip);
    }

    EXPECT_FALSE(mitigation.allow_connection(ip));
    EXPECT_TRUE(mitigation.is_blocked(ip));
}

TEST(SecurityTest, SybilDefenseCanValidateWithLowDifficultySetting) {
    SybilDefense sybil;
    sybil.set_min_pow_difficulty(0);

    const bool accepted = sybil.validate_new_identity(make_node(7), make_hash(9));
    EXPECT_TRUE(accepted);
}

TEST(SecurityTest, AccessControlRespectsKeysReputationAndFounderRole) {
    const NodeID local = make_node(11);
    const Hash256 network_id = make_hash(55);

    Ledger ledger(local);
    ledger.record_node_joined(local);
    ledger.record_key_issued(core::KeyType::IDENTITY, 1, IssuanceMethod::POW, make_hash(21));
    ledger.record_key_issued(core::KeyType::NETWORK, 3, IssuanceMethod::POW, make_hash(22));
    ledger.record_reputation_update(local, 120, "trusted bootstrap");
    ledger.record_network_created(network_id);
    ledger.record_network_member_added(network_id, local, "FOUNDER");

    StateManager state(ledger);
    AccessControl access(state);

    EXPECT_TRUE(access.can_view_content(local));
    EXPECT_TRUE(access.can_post_content(local));
    EXPECT_TRUE(access.can_create_network(local));
    EXPECT_TRUE(access.can_issue_invitation(local, network_id));
    EXPECT_TRUE(access.can_revoke_keys(local, network_id));
    EXPECT_TRUE(access.can_disband_network(local, network_id));
    EXPECT_EQ(access.get_access_level(local), AccessLevel::TRUSTED);
    EXPECT_EQ(access.get_access_level_in_network(local, network_id), AccessLevel::FOUNDER);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
