#include "network/network.hpp"
#include "core/node/node_identity.hpp"
#include "crypto/blake3.hpp"
#include "crypto/ed25519.hpp"
#include <gtest/gtest.h>
#include <filesystem>

using namespace cashew;
using namespace cashew::network;
using namespace cashew::core;

namespace {

NodeID node_id_from_public_key(const PublicKey& pk) {
    bytes key_bytes(pk.begin(), pk.end());
    return NodeID(crypto::Blake3::hash(key_bytes));
}

ContentHash content_hash_from_text(const std::string& text) {
    bytes payload(text.begin(), text.end());
    return ContentHash(crypto::Blake3::hash(payload));
}

Network make_test_network() {
    Hash256 network_seed{};
    network_seed[0] = 0x42;
    return Network(cashew::network::NetworkID(network_seed), content_hash_from_text("network-test-content"));
}

} // namespace

class NetworkTest : public ::testing::Test {
protected:
    std::pair<PublicKey, SecretKey> founder_kp;
    std::pair<PublicKey, SecretKey> invitee_kp;
    NodeID founder_id;
    NodeID invitee_id;

    void SetUp() override {
        founder_kp = crypto::Ed25519::generate_keypair();
        invitee_kp = crypto::Ed25519::generate_keypair();
        founder_id = node_id_from_public_key(founder_kp.first);
        invitee_id = node_id_from_public_key(invitee_kp.first);
    }
};

TEST_F(NetworkTest, AddAndRemoveMember) {
    auto network = make_test_network();

    NetworkMember founder(founder_id, founder_kp.first, MemberRole::FOUNDER);
    EXPECT_TRUE(network.add_member(founder));
    EXPECT_FALSE(network.add_member(founder));
    EXPECT_EQ(network.member_count(), 1u);

    EXPECT_TRUE(network.remove_member(founder_id));
    EXPECT_FALSE(network.remove_member(founder_id));
    EXPECT_EQ(network.member_count(), 0u);
}

TEST_F(NetworkTest, InvitationVerifyAndAccept) {
    auto network = make_test_network();

    NetworkMember founder(founder_id, founder_kp.first, MemberRole::FOUNDER);
    ASSERT_TRUE(network.add_member(founder));

    auto invitation = network.create_invitation(
        founder_id,
        invitee_id,
        founder_kp.first,
        founder_kp.second,
        MemberRole::FULL,
        std::chrono::minutes(5)
    );

    EXPECT_TRUE(network.verify_invitation(invitation));
    EXPECT_TRUE(network.accept_invitation(invitation));

    auto invitee_member = network.get_member(invitee_id);
    ASSERT_TRUE(invitee_member.has_value());
    EXPECT_EQ(invitee_member->role, MemberRole::FULL);
}

TEST_F(NetworkTest, InvitationVerificationFailsOnTamper) {
    auto network = make_test_network();
    NetworkMember founder(founder_id, founder_kp.first, MemberRole::FOUNDER);
    ASSERT_TRUE(network.add_member(founder));

    auto invitation = network.create_invitation(
        founder_id,
        invitee_id,
        founder_kp.first,
        founder_kp.second,
        MemberRole::OBSERVER,
        std::chrono::minutes(5)
    );

    invitation.proposed_role = MemberRole::FULL;
    EXPECT_FALSE(network.verify_invitation(invitation));
}

TEST_F(NetworkTest, SerializeDeserializeRoundTrip) {
    auto network = make_test_network();
    NetworkMember founder(founder_id, founder_kp.first, MemberRole::FOUNDER);
    ASSERT_TRUE(network.add_member(founder));

    auto bytes = network.serialize();
    auto restored = Network::deserialize(bytes);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->get_id().id, network.get_id().id);
    EXPECT_EQ(restored->get_thing_hash().hash, network.get_thing_hash().hash);
    EXPECT_EQ(restored->member_count(), network.member_count());
}

TEST_F(NetworkTest, RegistrySaveLoadRoundTrip) {
    auto network = make_test_network();
    NetworkMember founder(founder_id, founder_kp.first, MemberRole::FOUNDER);
    ASSERT_TRUE(network.add_member(founder));

    NetworkRegistry registry;
    ASSERT_TRUE(registry.add_network(network));

    const auto base = std::filesystem::path("./test_network_data");
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    ASSERT_TRUE(registry.save_to_disk(base.string()));

    NetworkRegistry loaded;
    ASSERT_TRUE(loaded.load_from_disk(base.string()));
    EXPECT_EQ(loaded.total_network_count(), 1u);

    std::filesystem::remove_all(base);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
