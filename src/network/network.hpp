#pragma once

#include "cashew/common.hpp"
#include "core/thing/thing.hpp"
#include "core/keys/key.hpp"
#include <vector>
#include <optional>
#include <chrono>

namespace cashew::network {

/**
 * NetworkID - Unique identifier for a network hosting a single Thing
 */
struct NetworkID {
    Hash256 id;
    
    NetworkID() = default;
    explicit NetworkID(const Hash256& hash) : id(hash) {}
    
    bool operator==(const NetworkID& other) const { return id == other.id; }
    bool operator<(const NetworkID& other) const { return id < other.id; }
};

/**
 * MemberRole - Role of a member in the network
 */
enum class MemberRole : uint8_t {
    FOUNDER = 0,    // Created the network
    FULL = 1,       // Full hosting member (has complete Thing replica)
    PENDING = 2,    // Invited but not yet accepted
    OBSERVER = 3    // Read-only, doesn't host
};

/**
 * NetworkMember - Information about a network participant
 */
struct NetworkMember {
    NodeID node_id;
    PublicKey public_key;
    MemberRole role;
    uint64_t joined_timestamp;
    uint64_t last_seen_timestamp;
    bool has_complete_replica;
    float reliability_score;  // 0.0 to 1.0
    
    NetworkMember() = default;
    NetworkMember(const NodeID& id, const PublicKey& key, MemberRole r)
        : node_id(id), public_key(key), role(r),
          joined_timestamp(0), last_seen_timestamp(0),
          has_complete_replica(false), reliability_score(1.0f) {}
};

/**
 * NetworkInvitation - Invitation to join a network
 */
struct NetworkInvitation {
    NetworkID network_id;
    NodeID inviter_id;
    NodeID invitee_id;
    MemberRole proposed_role;
    uint64_t expires_timestamp;
    Signature signature;  // Signed by inviter
    
    // Serialize for signing
    std::vector<uint8_t> to_bytes() const;
};

/**
 * NetworkQuorum - Redundancy and health settings for a network
 */
struct NetworkQuorum {
    size_t min_replicas;      // Minimum healthy replicas required
    size_t target_replicas;   // Target number of replicas
    size_t max_replicas;      // Maximum replicas allowed
    
    static constexpr size_t DEFAULT_MIN = 3;
    static constexpr size_t DEFAULT_TARGET = 5;
    static constexpr size_t DEFAULT_MAX = 10;
    
    NetworkQuorum()
        : min_replicas(DEFAULT_MIN),
          target_replicas(DEFAULT_TARGET),
          max_replicas(DEFAULT_MAX) {}
    
    bool is_healthy(size_t current_replicas) const {
        return current_replicas >= min_replicas;
    }
    
    bool needs_replication(size_t current_replicas) const {
        return current_replicas < target_replicas;
    }
    
    bool at_capacity(size_t current_replicas) const {
        return current_replicas >= max_replicas;
    }
};

/**
 * NetworkHealth - Current health status of the network
 */
enum class NetworkHealth : uint8_t {
    CRITICAL = 0,   // Below minimum replicas
    DEGRADED = 1,   // At minimum but below target
    HEALTHY = 2,    // At or above target
    OPTIMAL = 3     // At target with all members active
};

/**
 * Network - A cluster of nodes hosting a single Thing
 * 
 * Key properties:
 * - Each Network hosts exactly ONE Thing
 * - Invitation-only membership
 * - Automatic redundancy management
 * - Quorum-based replication
 * - Dynamic scaling within limits
 */
class Network {
public:
    Network(const NetworkID& id, const ContentHash& thing_hash);
    ~Network() = default;
    
    // Identity
    NetworkID get_id() const { return network_id_; }
    ContentHash get_thing_hash() const { return thing_hash_; }
    
    // Membership management
    bool add_member(const NetworkMember& member);
    bool remove_member(const NodeID& node_id);
    std::optional<NetworkMember> get_member(const NodeID& node_id) const;
    std::vector<NetworkMember> get_all_members() const;
    size_t member_count() const;
    size_t active_replica_count() const;
    
    // Invitation workflow
    NetworkInvitation create_invitation(
        const NodeID& inviter_id,
        const NodeID& invitee_id,
        MemberRole role,
        std::chrono::seconds valid_duration = std::chrono::hours(24)
    );
    
    bool verify_invitation(const NetworkInvitation& invitation) const;
    bool accept_invitation(const NetworkInvitation& invitation);
    bool reject_invitation(const NetworkInvitation& invitation);
    
    // Quorum management
    void set_quorum(const NetworkQuorum& quorum) { quorum_ = quorum; }
    NetworkQuorum get_quorum() const { return quorum_; }
    
    // Health monitoring
    NetworkHealth get_health() const;
    bool is_healthy() const;
    bool needs_new_replicas() const;
    bool can_accept_member() const;
    
    // Member reliability
    void update_member_reliability(const NodeID& node_id, float score);
    void mark_member_active(const NodeID& node_id);
    void mark_replica_complete(const NodeID& node_id, bool complete);
    
    // Replication coordination
    std::vector<NodeID> get_replication_candidates() const;
    NodeID select_best_source_for_replication() const;
    
    // Redundancy adjustment
    bool adjust_redundancy();  // Returns true if changes were made
    size_t calculate_target_redundancy() const;  // Dynamic redundancy calculation
    bool should_add_replicas() const;
    bool should_remove_replicas() const;
    std::vector<NodeID> select_nodes_for_removal() const;  // Select lowest reliability nodes
    
    // Network lifecycle
    bool should_dissolve() const;  // Too few members, dissolve network
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static std::optional<Network> deserialize(const std::vector<uint8_t>& data);

private:
    NetworkID network_id_;
    ContentHash thing_hash_;
    std::vector<NetworkMember> members_;
    NetworkQuorum quorum_;
    uint64_t created_timestamp_;
    
    // Pending invitations
    struct PendingInvitation {
        NetworkInvitation invitation;
        uint64_t created_timestamp;
    };
    std::vector<PendingInvitation> pending_invitations_;
    
    // Constants
    static constexpr uint64_t MEMBER_TIMEOUT_SECONDS = 3600;  // 1 hour
    static constexpr float MIN_RELIABILITY_SCORE = 0.5f;
    
    // Helper methods
    bool is_member_active(const NetworkMember& member) const;
    void cleanup_expired_invitations();
};

/**
 * NetworkRegistry - Manages all networks this node is part of
 */
class NetworkRegistry {
public:
    NetworkRegistry() = default;
    ~NetworkRegistry() = default;
    
    // Network lifecycle
    NetworkID create_network(const ContentHash& thing_hash);
    bool add_network(const Network& network);
    bool remove_network(const NetworkID& network_id);
    
    // Network lookup
    std::optional<Network> get_network(const NetworkID& network_id) const;
    std::vector<Network> get_all_networks() const;
    std::vector<Network> get_networks_for_thing(const ContentHash& thing_hash) const;
    
    // Membership queries
    bool is_member_of(const NetworkID& network_id) const;
    std::vector<NetworkID> get_joined_networks() const;
    
    // Health overview
    size_t healthy_network_count() const;
    size_t total_network_count() const;
    
    // Persistence
    bool save_to_disk(const std::string& directory);
    bool load_from_disk(const std::string& directory);

private:
    std::vector<Network> networks_;
    
    // Next network ID (incremental for this node)
    uint64_t next_network_counter_ = 0;
};

/**
 * ReplicationRequest - Request to replicate Thing data
 */
struct ReplicationRequest {
    NetworkID network_id;
    ContentHash thing_hash;
    NodeID source_node;    // Node to replicate from
    NodeID target_node;    // Node to replicate to
    uint64_t request_timestamp;
    uint32_t priority;     // 0=low, 5=normal, 10=urgent
    
    bool operator<(const ReplicationRequest& other) const {
        return priority < other.priority;  // Lower priority = less urgent
    }
};

/**
 * ReplicationStatus - Status of an ongoing replication
 */
enum class ReplicationStatus : uint8_t {
    PENDING = 0,      // Request created, not started
    IN_PROGRESS = 1,  // Data transfer in progress
    VERIFYING = 2,    // Verifying hash integrity
    COMPLETED = 3,    // Successfully replicated
    FAILED = 4,       // Failed to replicate
    CANCELLED = 5     // Request cancelled
};

/**
 * ReplicationJob - Tracks a replication operation
 */
struct ReplicationJob {
    ReplicationRequest request;
    ReplicationStatus status;
    uint64_t started_timestamp;
    uint64_t completed_timestamp;
    size_t bytes_transferred;
    std::string error_message;
    uint32_t retry_count;
};

/**
 * ReplicationCoordinator - Manages Thing replication across networks
 * 
 * Responsibilities:
 * - Coordinate replication of Things to maintain redundancy
 * - Prioritize urgent replication needs (critical networks)
 * - Track replication progress and retry failures
 * - Verify integrity of replicated data
 * - Balance replication load across nodes
 */
class ReplicationCoordinator {
public:
    ReplicationCoordinator() = default;
    ~ReplicationCoordinator() = default;
    
    // Request management
    void request_replication(const ReplicationRequest& request);
    void cancel_replication(const NetworkID& network_id, const NodeID& target_node);
    
    // Scheduling
    std::optional<ReplicationJob> get_next_job();
    void mark_job_started(const ReplicationRequest& request);
    void mark_job_completed(const ReplicationRequest& request, bool success, const std::string& error = "");
    
    // Status queries
    std::vector<ReplicationJob> get_active_jobs() const;
    std::vector<ReplicationJob> get_pending_jobs() const;
    std::vector<ReplicationJob> get_failed_jobs() const;
    size_t pending_job_count() const;
    size_t active_job_count() const;
    
    // Job management
    void retry_failed_jobs(uint32_t max_retries = 3);
    void cleanup_old_jobs(uint64_t max_age_seconds = 86400);  // 24 hours
    
    // Statistics
    struct ReplicationStats {
        size_t total_requests;
        size_t completed_successfully;
        size_t failed;
        size_t in_progress;
        size_t pending;
        uint64_t total_bytes_transferred;
        float average_completion_time_seconds;
    };
    
    ReplicationStats get_stats() const;
    
    // Constants
    static constexpr uint32_t MAX_CONCURRENT_JOBS = 5;
    static constexpr uint32_t DEFAULT_PRIORITY = 5;
    static constexpr uint32_t MAX_RETRIES = 3;
    static constexpr uint64_t JOB_TIMEOUT_SECONDS = 3600;  // 1 hour

private:
    std::vector<ReplicationJob> jobs_;
    
    void prioritize_jobs();  // Sort jobs by priority
    bool can_start_new_job() const;
};

} // namespace cashew::network
