#include "network.hpp"
#include "utils/logger.hpp"
#include "crypto/blake3.hpp"
#include "crypto/random.hpp"
#include <algorithm>
#include <cstring>

namespace cashew::network {

// NetworkInvitation implementation

std::vector<uint8_t> NetworkInvitation::to_bytes() const {
    std::vector<uint8_t> data;
    data.reserve(32 + 32 + 32 + 1 + 8);
    
    // Network ID
    data.insert(data.end(), network_id.id.begin(), network_id.id.end());
    
    // Inviter ID
    data.insert(data.end(), inviter_id.id.begin(), inviter_id.id.end());
    
    // Invitee ID
    data.insert(data.end(), invitee_id.id.begin(), invitee_id.id.end());
    
    // Role
    data.push_back(static_cast<uint8_t>(proposed_role));
    
    // Expiration timestamp
    for (int i = 0; i < 8; ++i) {
        data.push_back((expires_timestamp >> (i * 8)) & 0xFF);
    }
    
    return data;
}

// Network implementation

Network::Network(const NetworkID& id, const ContentHash& thing_hash)
    : network_id_(id), thing_hash_(thing_hash),
      created_timestamp_(std::chrono::system_clock::now().time_since_epoch().count()) {
    CASHEW_LOG_INFO("Created network {} for Thing {}",
                    crypto::Blake3::hash_to_hex(network_id_.id),
                    crypto::Blake3::hash_to_hex(thing_hash_.hash));
}

bool Network::add_member(const NetworkMember& member) {
    // Check if already a member
    for (const auto& m : members_) {
        if (m.node_id == member.node_id) {
            CASHEW_LOG_WARN("Node {} already a member of network",
                           crypto::Blake3::hash_to_hex(member.node_id.id));
            return false;
        }
    }
    
    // Check capacity
    if (quorum_.at_capacity(members_.size())) {
        CASHEW_LOG_WARN("Network at capacity ({} members)", members_.size());
        return false;
    }
    
    members_.push_back(member);
    CASHEW_LOG_INFO("Added member {} to network (role: {})",
                   crypto::Blake3::hash_to_hex(member.node_id.id),
                   static_cast<int>(member.role));
    
    return true;
}

bool Network::remove_member(const NodeID& node_id) {
    auto it = std::remove_if(members_.begin(), members_.end(),
        [&node_id](const NetworkMember& m) { return m.node_id == node_id; });
    
    if (it == members_.end()) {
        return false;
    }
    
    members_.erase(it, members_.end());
    CASHEW_LOG_INFO("Removed member {} from network",
                   crypto::Blake3::hash_to_hex(node_id.id));
    
    return true;
}

std::optional<NetworkMember> Network::get_member(const NodeID& node_id) const {
    for (const auto& member : members_) {
        if (member.node_id == node_id) {
            return member;
        }
    }
    return std::nullopt;
}

std::vector<NetworkMember> Network::get_all_members() const {
    return members_;
}

size_t Network::member_count() const {
    return members_.size();
}

size_t Network::active_replica_count() const {
    size_t count = 0;
    for (const auto& member : members_) {
        if (member.has_complete_replica && is_member_active(member)) {
            ++count;
        }
    }
    return count;
}

NetworkInvitation Network::create_invitation(
    const NodeID& inviter_id,
    const NodeID& invitee_id,
    MemberRole role,
    std::chrono::seconds valid_duration) {
    
    NetworkInvitation invitation;
    invitation.network_id = network_id_;
    invitation.inviter_id = inviter_id;
    invitation.invitee_id = invitee_id;
    invitation.proposed_role = role;
    
    auto now = std::chrono::system_clock::now();
    auto expiration = now + valid_duration;
    invitation.expires_timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(expiration.time_since_epoch()).count();
    
    // TODO: Sign invitation with inviter's key
    // For now, just create a placeholder signature
    invitation.signature = Signature{};
    
    // Add to pending invitations
    PendingInvitation pending;
    pending.invitation = invitation;
    pending.created_timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    pending_invitations_.push_back(pending);
    
    CASHEW_LOG_INFO("Created invitation for {} to join network as {}",
                   crypto::Blake3::hash_to_hex(invitee_id.id),
                   static_cast<int>(role));
    
    return invitation;
}

bool Network::verify_invitation(const NetworkInvitation& invitation) const {
    // Check network ID matches
    if (invitation.network_id.id != network_id_.id) {
        return false;
    }
    
    // Check not expired
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    if (now_seconds > invitation.expires_timestamp) {
        return false;
    }
    
    // TODO: Verify signature with inviter's public key
    
    return true;
}

bool Network::accept_invitation(const NetworkInvitation& invitation) {
    if (!verify_invitation(invitation)) {
        CASHEW_LOG_WARN("Invalid invitation");
        return false;
    }
    
    // Create new member
    NetworkMember member(invitation.invitee_id, PublicKey{}, invitation.proposed_role);
    
    auto now = std::chrono::system_clock::now();
    member.joined_timestamp = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    member.last_seen_timestamp = member.joined_timestamp;
    
    return add_member(member);
}

bool Network::reject_invitation(const NetworkInvitation& invitation) {
    // Remove from pending invitations
    auto it = std::remove_if(pending_invitations_.begin(), pending_invitations_.end(),
        [&invitation](const PendingInvitation& p) {
            return p.invitation.invitee_id == invitation.invitee_id &&
                   p.invitation.network_id.id == invitation.network_id.id;
        });
    
    if (it == pending_invitations_.end()) {
        return false;
    }
    
    pending_invitations_.erase(it, pending_invitations_.end());
    CASHEW_LOG_INFO("Invitation rejected by {}",
                   crypto::Blake3::hash_to_hex(invitation.invitee_id.id));
    
    return true;
}

NetworkHealth Network::get_health() const {
    size_t replicas = active_replica_count();
    
    if (replicas < quorum_.min_replicas) {
        return NetworkHealth::CRITICAL;
    } else if (replicas < quorum_.target_replicas) {
        return NetworkHealth::DEGRADED;
    } else if (replicas == quorum_.target_replicas) {
        // Check if all members are active
        bool all_active = true;
        for (const auto& member : members_) {
            if (member.has_complete_replica && !is_member_active(member)) {
                all_active = false;
                break;
            }
        }
        return all_active ? NetworkHealth::OPTIMAL : NetworkHealth::HEALTHY;
    } else {
        return NetworkHealth::HEALTHY;
    }
}

bool Network::is_healthy() const {
    return get_health() != NetworkHealth::CRITICAL;
}

bool Network::needs_new_replicas() const {
    return quorum_.needs_replication(active_replica_count());
}

bool Network::can_accept_member() const {
    return !quorum_.at_capacity(members_.size());
}

void Network::update_member_reliability(const NodeID& node_id, float score) {
    for (auto& member : members_) {
        if (member.node_id == node_id) {
            member.reliability_score = std::clamp(score, 0.0f, 1.0f);
            CASHEW_LOG_DEBUG("Updated reliability for {}: {}",
                           crypto::Blake3::hash_to_hex(node_id.id),
                           member.reliability_score);
            break;
        }
    }
}

void Network::mark_member_active(const NodeID& node_id) {
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    for (auto& member : members_) {
        if (member.node_id == node_id) {
            member.last_seen_timestamp = now_seconds;
            break;
        }
    }
}

void Network::mark_replica_complete(const NodeID& node_id, bool complete) {
    for (auto& member : members_) {
        if (member.node_id == node_id) {
            member.has_complete_replica = complete;
            CASHEW_LOG_INFO("Member {} replica status: {}",
                           crypto::Blake3::hash_to_hex(node_id.id),
                           complete ? "complete" : "incomplete");
            break;
        }
    }
}

std::vector<NodeID> Network::get_replication_candidates() const {
    std::vector<NodeID> candidates;
    
    for (const auto& member : members_) {
        // Must be active, reliable, and have complete replica
        if (is_member_active(member) &&
            member.has_complete_replica &&
            member.reliability_score >= MIN_RELIABILITY_SCORE) {
            candidates.push_back(member.node_id);
        }
    }
    
    return candidates;
}

NodeID Network::select_best_source_for_replication() const {
    auto candidates = get_replication_candidates();
    
    if (candidates.empty()) {
        return NodeID{};
    }
    
    // Find member with highest reliability score
    const NetworkMember* best = nullptr;
    for (const auto& member : members_) {
        for (const auto& candidate_id : candidates) {
            if (member.node_id == candidate_id) {
                if (!best || member.reliability_score > best->reliability_score) {
                    best = &member;
                }
                break;
            }
        }
    }
    
    return best ? best->node_id : NodeID{};
}

bool Network::should_dissolve() const {
    // Dissolve if we can't maintain minimum quorum
    size_t healthy_members = 0;
    for (const auto& member : members_) {
        if (is_member_active(member) && 
            member.reliability_score >= MIN_RELIABILITY_SCORE) {
            ++healthy_members;
        }
    }
    
    return healthy_members < quorum_.min_replicas;
}

std::vector<uint8_t> Network::serialize() const {
    // TODO: Implement full serialization
    // For now, just return empty vector
    return std::vector<uint8_t>();
}

std::optional<Network> Network::deserialize(const std::vector<uint8_t>& data) {
    (void)data;
    // TODO: Implement full deserialization
    return std::nullopt;
}

bool Network::is_member_active(const NetworkMember& member) const {
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    uint64_t elapsed = now_seconds - member.last_seen_timestamp;
    return elapsed < MEMBER_TIMEOUT_SECONDS;
}

void Network::cleanup_expired_invitations() {
    auto now = std::chrono::system_clock::now();
    uint64_t now_seconds = 
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    auto it = std::remove_if(pending_invitations_.begin(), pending_invitations_.end(),
        [now_seconds](const PendingInvitation& p) {
            return now_seconds > p.invitation.expires_timestamp;
        });
    
    pending_invitations_.erase(it, pending_invitations_.end());
}

// NetworkRegistry implementation

NetworkID NetworkRegistry::create_network(const ContentHash& thing_hash) {
    // Generate network ID from thing hash + counter
    std::vector<uint8_t> id_input;
    id_input.insert(id_input.end(), 
                   thing_hash.hash.begin(), 
                   thing_hash.hash.end());
    
    // Add counter
    for (int i = 0; i < 8; ++i) {
        id_input.push_back((next_network_counter_ >> (i * 8)) & 0xFF);
    }
    
    auto id_hash = crypto::Blake3::hash(id_input);
    NetworkID network_id(id_hash);
    
    Network network(network_id, thing_hash);
    networks_.push_back(network);
    
    ++next_network_counter_;
    
    CASHEW_LOG_INFO("Created new network {} (total: {})",
                   crypto::Blake3::hash_to_hex(network_id.id),
                   networks_.size());
    
    return network_id;
}

bool NetworkRegistry::add_network(const Network& network) {
    // Check if network already exists
    for (const auto& n : networks_) {
        if (n.get_id() == network.get_id()) {
            return false;
        }
    }
    
    networks_.push_back(network);
    return true;
}

bool NetworkRegistry::remove_network(const NetworkID& network_id) {
    auto it = std::remove_if(networks_.begin(), networks_.end(),
        [&network_id](const Network& n) { return n.get_id() == network_id; });
    
    if (it == networks_.end()) {
        return false;
    }
    
    networks_.erase(it, networks_.end());
    return true;
}

std::optional<Network> NetworkRegistry::get_network(const NetworkID& network_id) const {
    for (const auto& network : networks_) {
        if (network.get_id() == network_id) {
            return network;
        }
    }
    return std::nullopt;
}

std::vector<Network> NetworkRegistry::get_all_networks() const {
    return networks_;
}

std::vector<Network> NetworkRegistry::get_networks_for_thing(const ContentHash& thing_hash) const {
    std::vector<Network> result;
    
    for (const auto& network : networks_) {
        if (network.get_thing_hash().hash == thing_hash.hash) {
            result.push_back(network);
        }
    }
    
    return result;
}

bool NetworkRegistry::is_member_of(const NetworkID& network_id) const {
    return get_network(network_id).has_value();
}

std::vector<NetworkID> NetworkRegistry::get_joined_networks() const {
    std::vector<NetworkID> ids;
    for (const auto& network : networks_) {
        ids.push_back(network.get_id());
    }
    return ids;
}

size_t NetworkRegistry::healthy_network_count() const {
    size_t count = 0;
    for (const auto& network : networks_) {
        if (network.is_healthy()) {
            ++count;
        }
    }
    return count;
}

size_t NetworkRegistry::total_network_count() const {
    return networks_.size();
}

bool NetworkRegistry::save_to_disk(const std::string& directory) {
    (void)directory;
    // TODO: Implement persistence
    return false;
}

bool NetworkRegistry::load_from_disk(const std::string& directory) {
    (void)directory;
    // TODO: Implement persistence
    return false;
}

} // namespace cashew::network
