#pragma once

#include "cashew/common.hpp"
#include "core/ledger/ledger.hpp"
#include "core/reputation/reputation.hpp"
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace cashew::security {

/**
 * RateLimitPolicy - Configure rate limiting per operation
 */
struct RateLimitPolicy {
    size_t max_requests_per_minute;
    size_t max_requests_per_hour;
    size_t burst_size;  // Allow bursts up to this size
    
    RateLimitPolicy()
        : max_requests_per_minute(60),
          max_requests_per_hour(1000),
          burst_size(10) {}
};

/**
 * RateLimiter - Token bucket rate limiter
 * 
 * Prevents:
 * - Request flooding
 * - Resource exhaustion
 * - Bandwidth abuse
 */
class RateLimiter {
public:
    explicit RateLimiter(const RateLimitPolicy& policy = RateLimitPolicy());
    ~RateLimiter() = default;
    
    /**
     * Check if request is allowed
     * @param identifier Client/peer identifier
     * @return true if request allowed, false if rate limited
     */
    bool allow_request(const NodeID& identifier);
    
    /**
     * Reset limits for identifier
     */
    void reset(const NodeID& identifier);
    
    /**
     * Cleanup old entries
     */
    void cleanup_stale_entries();
    
    // Statistics
    uint64_t get_total_requests() const { return total_requests_; }
    uint64_t get_blocked_requests() const { return blocked_requests_; }
    
private:
    struct TokenBucket {
        double tokens;
        uint64_t last_update_time;
        size_t requests_this_minute;
        size_t requests_this_hour;
        uint64_t minute_window_start;
        uint64_t hour_window_start;
        
        TokenBucket() 
            : tokens(0), last_update_time(0),
              requests_this_minute(0), requests_this_hour(0),
              minute_window_start(0), hour_window_start(0) {}
    };
    
    RateLimitPolicy policy_;
    std::map<NodeID, TokenBucket> buckets_;
    uint64_t total_requests_;
    uint64_t blocked_requests_;
    
    void refill_tokens(TokenBucket& bucket, uint64_t current_time);
    uint64_t current_timestamp() const;
};

/**
 * SybilDefense - Detect and prevent Sybil attacks
 * 
 * Techniques:
 * - PoW cost for new identities
 * - Reputation requirements
 * - Network connectivity analysis
 * - Behavioral pattern detection
 */
class SybilDefense {
public:
    SybilDefense();
    ~SybilDefense() = default;
    
    /**
     * Validate new node identity
     * @param node_id Node attempting to join
     * @param pow_proof Proof of work
     * @return true if valid, false if suspected Sybil
     */
    bool validate_new_identity(
        const NodeID& node_id,
        const Hash256& pow_proof
    );
    
    /**
     * Check if node exhibits Sybil patterns
     * @param node_id Node to check
     * @param connections Number of connections
     * @param join_time When node joined
     * @return Sybil suspicion score (0.0 = clean, 1.0 = highly suspicious)
     */
    float calculate_sybil_score(
        const NodeID& node_id,
        size_t connections,
        uint64_t join_time
    ) const;
    
    /**
     * Detect coordinated Sybil group
     * @param nodes Set of nodes to analyze
     * @return Suspected Sybil groups
     */
    std::vector<std::set<NodeID>> detect_sybil_groups(
        const std::vector<NodeID>& nodes
    ) const;
    
    /**
     * Record node behavior
     */
    void record_node_activity(const NodeID& node_id, const std::string& activity);
    
    // Configuration
    void set_min_pow_difficulty(uint32_t difficulty) { min_pow_difficulty_ = difficulty; }
    void set_min_reputation(int32_t reputation) { min_reputation_ = reputation; }
    
private:
    uint32_t min_pow_difficulty_;
    int32_t min_reputation_;
    std::map<NodeID, std::vector<std::string>> activity_log_;
    
    bool verify_pow_difficulty(const Hash256& pow_proof, uint32_t difficulty) const;
};

/**
 * DDoSMitigation - Protect against distributed denial of service
 * 
 * Strategies:
 * - Request rate limiting
 * - Connection limiting per IP
 * - Challenge-response for suspicious traffic
 * - Adaptive throttling
 */
class DDoSMitigation {
public:
    DDoSMitigation();
    ~DDoSMitigation() = default;
    
    /**
     * Check if connection should be allowed
     * @param ip_address Source IP
     * @return true if allowed, false if blocked
     */
    bool allow_connection(const std::string& ip_address);
    
    /**
     * Record connection from IP
     */
    void record_connection(const std::string& ip_address);
    
    /**
     * Record connection close
     */
    void close_connection(const std::string& ip_address);
    
    /**
     * Block IP address
     * @param duration_seconds How long to block
     */
    void block_ip(const std::string& ip_address, uint64_t duration_seconds);
    
    /**
     * Check if IP is blocked
     */
    bool is_blocked(const std::string& ip_address) const;
    
    /**
     * Detect DDoS attack pattern
     * @return true if attack detected
     */
    bool detect_attack_pattern() const;
    
    /**
     * Get current threat level (0.0 = normal, 1.0 = severe attack)
     */
    float get_threat_level() const;
    
    /**
     * Cleanup expired blocks
     */
    void cleanup_expired_blocks();
    
    // Statistics
    uint64_t get_total_connections() const { return total_connections_; }
    uint64_t get_blocked_connections() const { return blocked_connections_; }
    size_t get_blocked_ips_count() const { return blocked_ips_.size(); }
    
private:
    struct IPConnectionInfo {
        size_t active_connections;
        uint64_t total_connections;
        uint64_t last_connection_time;
        std::vector<uint64_t> connection_timestamps;  // Last 100 connections
        
        IPConnectionInfo() 
            : active_connections(0), total_connections(0), last_connection_time(0) {}
    };
    
    struct BlockEntry {
        uint64_t block_time;
        uint64_t expiry_time;
        std::string reason;
    };
    
    std::map<std::string, IPConnectionInfo> ip_connections_;
    std::map<std::string, BlockEntry> blocked_ips_;
    uint64_t total_connections_;
    uint64_t blocked_connections_;
    
    static constexpr size_t MAX_CONNECTIONS_PER_IP = 10;
    static constexpr size_t ATTACK_THRESHOLD_CONNECTIONS_PER_MINUTE = 50;
    
    uint64_t current_timestamp() const;
    bool exceeds_connection_limit(const IPConnectionInfo& info) const;
};

/**
 * ForkDetector - Detect identity fork attacks
 * 
 * Fork attack: Same node ID with different keys/signatures
 * 
 * Detection:
 * - Monitor for conflicting signatures
 * - Track key rotation history
 * - Gossip-based consensus on node state
 */
class ForkDetector {
public:
    explicit ForkDetector(ledger::Ledger& ledger);
    ~ForkDetector() = default;
    
    /**
     * Check for identity fork
     * @param node_id Node to check
     * @param claimed_key Public key claimed by node
     * @return true if fork detected
     */
    bool detect_fork(const NodeID& node_id, const PublicKey& claimed_key);
    
    /**
     * Verify signature consistency
     * @param node_id Node that signed
     * @param message Message that was signed
     * @param signature Signature to verify
     * @return true if consistent with known key
     */
    bool verify_signature_consistency(
        const NodeID& node_id,
        const std::vector<uint8_t>& message,
        const Signature& signature
    );
    
    /**
     * Record valid key for node
     */
    void record_node_key(const NodeID& node_id, const PublicKey& public_key);
    
    /**
     * Get all detected forks
     */
    std::vector<NodeID> get_detected_forks() const;
    
    /**
     * Mark node as forked
     */
    void mark_as_forked(const NodeID& node_id, const std::string& reason);
    
    // Statistics
    size_t get_fork_count() const { return forked_nodes_.size(); }
    
private:
    struct KeyRecord {
        PublicKey public_key;
        uint64_t first_seen_time;
        uint64_t last_seen_time;
        size_t signature_count;
    };
    
    ledger::Ledger& ledger_;
    std::map<NodeID, std::vector<KeyRecord>> node_keys_;
    std::set<NodeID> forked_nodes_;
    
    uint64_t current_timestamp() const;
};

/**
 * AttackPreventionCoordinator - Unified attack prevention system
 * 
 * Integrates:
 * - Rate limiting
 * - Sybil defense
 * - DDoS mitigation
 * - Fork detection
 */
class AttackPreventionCoordinator {
public:
    AttackPreventionCoordinator(
        ledger::Ledger& ledger,
        reputation::ReputationManager& reputation_manager
    );
    ~AttackPreventionCoordinator() = default;
    
    // Connection validation
    bool validate_incoming_connection(const std::string& ip_address, const NodeID& node_id);
    void on_connection_established(const std::string& ip_address, const NodeID& node_id);
    void on_connection_closed(const std::string& ip_address, const NodeID& node_id);
    
    // Request validation
    bool validate_request(const NodeID& node_id, const std::string& request_type);
    
    // Identity validation
    bool validate_new_identity(const NodeID& node_id, const Hash256& pow_proof);
    bool validate_signature(
        const NodeID& node_id,
        const std::vector<uint8_t>& message,
        const Signature& signature
    );
    
    // Threat detection
    bool is_under_attack() const;
    float get_overall_threat_level() const;
    
    // Maintenance
    void tick();  // Call periodically
    void cleanup();
    
    // Statistics
    struct Statistics {
        uint64_t total_connections;
        uint64_t blocked_connections;
        uint64_t total_requests;
        uint64_t blocked_requests;
        size_t blocked_ips;
        size_t detected_forks;
        float threat_level;
        
        std::string to_string() const;
    };
    
    Statistics get_statistics() const;
    
    // Configuration
    void set_rate_limit_policy(const RateLimitPolicy& policy);
    void enable_sybil_defense(bool enable) { sybil_defense_enabled_ = enable; }
    void enable_ddos_mitigation(bool enable) { ddos_mitigation_enabled_ = enable; }
    void enable_fork_detection(bool enable) { fork_detection_enabled_ = enable; }
    
private:
    ledger::Ledger& ledger_;
    reputation::ReputationManager& reputation_manager_;
    
    RateLimiter rate_limiter_;
    SybilDefense sybil_defense_;
    DDoSMitigation ddos_mitigation_;
    ForkDetector fork_detector_;
    
    bool sybil_defense_enabled_;
    bool ddos_mitigation_enabled_;
    bool fork_detection_enabled_;
    
    uint64_t last_cleanup_time_;
    
    uint64_t current_timestamp() const;
};

} // namespace cashew::security
