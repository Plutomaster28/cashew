#include "security/attack_prevention.hpp"
#include "crypto/blake3.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <sstream>

namespace cashew::security {

// RateLimiter methods

RateLimiter::RateLimiter(const RateLimitPolicy& policy)
    : policy_(policy), total_requests_(0), blocked_requests_(0)
{
    CASHEW_LOG_INFO("RateLimiter initialized (max: {}/min, {}/hour)",
                   policy_.max_requests_per_minute,
                   policy_.max_requests_per_hour);
}

bool RateLimiter::allow_request(const NodeID& identifier) {
    total_requests_++;
    
    auto& bucket = buckets_[identifier];
    uint64_t current = current_timestamp();
    
    // Refill tokens
    refill_tokens(bucket, current);
    
    // Check minute window
    if (current - bucket.minute_window_start >= 60) {
        bucket.requests_this_minute = 0;
        bucket.minute_window_start = current;
    }
    
    // Check hour window
    if (current - bucket.hour_window_start >= 3600) {
        bucket.requests_this_hour = 0;
        bucket.hour_window_start = current;
    }
    
    // Check limits
    if (bucket.requests_this_minute >= policy_.max_requests_per_minute ||
        bucket.requests_this_hour >= policy_.max_requests_per_hour ||
        bucket.tokens < 1.0) {
        blocked_requests_++;
        CASHEW_LOG_WARN("Rate limit exceeded for peer");
        return false;
    }
    
    // Consume token
    bucket.tokens -= 1.0;
    bucket.requests_this_minute++;
    bucket.requests_this_hour++;
    
    return true;
}

void RateLimiter::reset(const NodeID& identifier) {
    buckets_.erase(identifier);
}

void RateLimiter::cleanup_stale_entries() {
    uint64_t current = current_timestamp();
    std::vector<NodeID> to_remove;
    
    for (const auto& [id, bucket] : buckets_) {
        if (current - bucket.last_update_time > 7200) {  // 2 hours
            to_remove.push_back(id);
        }
    }
    
    for (const auto& id : to_remove) {
        buckets_.erase(id);
    }
}

void RateLimiter::refill_tokens(TokenBucket& bucket, uint64_t current_time) {
    if (bucket.last_update_time == 0) {
        bucket.tokens = static_cast<double>(policy_.burst_size);
        bucket.last_update_time = current_time;
        bucket.minute_window_start = current_time;
        bucket.hour_window_start = current_time;
        return;
    }
    
    uint64_t elapsed = current_time - bucket.last_update_time;
    double tokens_to_add = elapsed * (static_cast<double>(policy_.max_requests_per_minute) / 60.0);
    
    bucket.tokens = std::min(
        bucket.tokens + tokens_to_add,
        static_cast<double>(policy_.burst_size)
    );
    bucket.last_update_time = current_time;
}

uint64_t RateLimiter::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

// SybilDefense methods

SybilDefense::SybilDefense()
    : min_pow_difficulty_(20),
      min_reputation_(0)
{
    CASHEW_LOG_INFO("SybilDefense initialized (min PoW difficulty: {})", min_pow_difficulty_);
}

bool SybilDefense::validate_new_identity(
    const NodeID& /* node_id */,
    const Hash256& pow_proof
) {
    // Verify PoW difficulty
    if (!verify_pow_difficulty(pow_proof, min_pow_difficulty_)) {
        CASHEW_LOG_WARN("Insufficient PoW difficulty for new identity");
        return false;
    }
    
    // Check if node_id derived from PoW proof
    std::vector<uint8_t> proof_vec(pow_proof.begin(), pow_proof.end());
    auto derived_hash = crypto::Blake3::hash(proof_vec);
    (void)derived_hash; // TODO: Verify node_id matches derived hash
    
    // Accept if proof is valid
    CASHEW_LOG_DEBUG("Validated new identity with PoW proof");
    return true;
}

float SybilDefense::calculate_sybil_score(
    const NodeID& node_id,
    size_t connections,
    uint64_t join_time
) const {
    float score = 0.0f;
    
    uint64_t current = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    uint64_t age_seconds = current - join_time;
    
    // New nodes with many connections are suspicious
    if (age_seconds < 3600 && connections > 20) {  // Less than 1 hour old
        score += 0.5f;
    }
    
    // Very high connection count is suspicious
    if (connections > 100) {
        score += 0.3f;
    }
    
    // Check activity patterns
    auto it = activity_log_.find(node_id);
    if (it != activity_log_.end()) {
        size_t activity_count = it->second.size();
        
        // Too little activity for age
        if (age_seconds > 86400 && activity_count < 10) {  // 1 day old, less than 10 activities
            score += 0.2f;
        }
    }
    
    return std::min(score, 1.0f);
}

std::vector<std::set<NodeID>> SybilDefense::detect_sybil_groups(
    const std::vector<NodeID>& /* nodes */
) const {
    // Simple clustering based on activity patterns
    // In production, use more sophisticated graph analysis
    
    std::vector<std::set<NodeID>> groups;
    
    // TODO: Implement graph-based Sybil detection
    // - Analyze connection patterns
    // - Detect suspicious clusters
    // - Use random walk-based algorithms
    
    return groups;
}

void SybilDefense::record_node_activity(const NodeID& node_id, const std::string& activity) {
    activity_log_[node_id].push_back(activity);
    
    // Keep only last 100 activities
    if (activity_log_[node_id].size() > 100) {
        activity_log_[node_id].erase(activity_log_[node_id].begin());
    }
}

bool SybilDefense::verify_pow_difficulty(const Hash256& pow_proof, uint32_t difficulty) const {
    // Count leading zero bits
    uint32_t leading_zeros = 0;
    
    for (const auto& byte : pow_proof) {
        if (byte == 0) {
            leading_zeros += 8;
        } else {
            // Count leading zeros in this byte
            uint8_t b = byte;
            while ((b & 0x80) == 0 && leading_zeros < difficulty) {
                leading_zeros++;
                b <<= 1;
            }
            break;
        }
        
        if (leading_zeros >= difficulty) {
            break;
        }
    }
    
    return leading_zeros >= difficulty;
}

// DDoSMitigation methods

DDoSMitigation::DDoSMitigation()
    : total_connections_(0), blocked_connections_(0)
{
    CASHEW_LOG_INFO("DDoSMitigation initialized (max {}/IP)", MAX_CONNECTIONS_PER_IP);
}

bool DDoSMitigation::allow_connection(const std::string& ip_address) {
    // Check if IP is blocked
    if (is_blocked(ip_address)) {
        blocked_connections_++;
        return false;
    }
    
    auto& info = ip_connections_[ip_address];
    
    // Check connection limit
    if (exceeds_connection_limit(info)) {
        CASHEW_LOG_WARN("Connection limit exceeded for IP: {}", ip_address);
        block_ip(ip_address, 3600);  // Block for 1 hour
        blocked_connections_++;
        return false;
    }
    
    return true;
}

void DDoSMitigation::record_connection(const std::string& ip_address) {
    auto& info = ip_connections_[ip_address];
    info.active_connections++;
    info.total_connections++;
    
    uint64_t current = current_timestamp();
    info.last_connection_time = current;
    info.connection_timestamps.push_back(current);
    
    // Keep only last 100 timestamps
    if (info.connection_timestamps.size() > 100) {
        info.connection_timestamps.erase(info.connection_timestamps.begin());
    }
    
    total_connections_++;
}

void DDoSMitigation::close_connection(const std::string& ip_address) {
    auto it = ip_connections_.find(ip_address);
    if (it != ip_connections_.end() && it->second.active_connections > 0) {
        it->second.active_connections--;
    }
}

void DDoSMitigation::block_ip(const std::string& ip_address, uint64_t duration_seconds) {
    uint64_t current = current_timestamp();
    
    BlockEntry entry;
    entry.block_time = current;
    entry.expiry_time = current + duration_seconds;
    entry.reason = "Rate limit exceeded";
    
    blocked_ips_[ip_address] = entry;
    
    CASHEW_LOG_WARN("Blocked IP {} for {}s", ip_address, duration_seconds);
}

bool DDoSMitigation::is_blocked(const std::string& ip_address) const {
    auto it = blocked_ips_.find(ip_address);
    if (it == blocked_ips_.end()) {
        return false;
    }
    
    uint64_t current = current_timestamp();
    return current < it->second.expiry_time;
}

bool DDoSMitigation::detect_attack_pattern() const {
    uint64_t current = current_timestamp();
    
    // Count connections in last minute
    size_t recent_connections = 0;
    
    for (const auto& [ip, info] : ip_connections_) {
        for (const auto& timestamp : info.connection_timestamps) {
            if (current - timestamp <= 60) {
                recent_connections++;
            }
        }
    }
    
    return recent_connections >= ATTACK_THRESHOLD_CONNECTIONS_PER_MINUTE;
}

float DDoSMitigation::get_threat_level() const {
    if (ip_connections_.empty()) {
        return 0.0f;
    }
    
    // Calculate based on blocked IPs ratio and connection rates
    float blocked_ratio = static_cast<float>(blocked_ips_.size()) / 
                         static_cast<float>(ip_connections_.size());
    
    return std::min(blocked_ratio * 2.0f, 1.0f);
}

void DDoSMitigation::cleanup_expired_blocks() {
    uint64_t current = current_timestamp();
    std::vector<std::string> to_unblock;
    
    for (const auto& [ip, entry] : blocked_ips_) {
        if (current >= entry.expiry_time) {
            to_unblock.push_back(ip);
        }
    }
    
    for (const auto& ip : to_unblock) {
        blocked_ips_.erase(ip);
        CASHEW_LOG_DEBUG("Unblocked IP: {}", ip);
    }
}

uint64_t DDoSMitigation::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

bool DDoSMitigation::exceeds_connection_limit(const IPConnectionInfo& info) const {
    return info.active_connections >= MAX_CONNECTIONS_PER_IP;
}

// ForkDetector methods

ForkDetector::ForkDetector(ledger::Ledger& ledger)
    : ledger_(ledger)
{
    CASHEW_LOG_INFO("ForkDetector initialized");
}

bool ForkDetector::detect_fork(const NodeID& node_id, const PublicKey& claimed_key) {
    auto it = node_keys_.find(node_id);
    
    if (it == node_keys_.end()) {
        // First time seeing this node, record key
        record_node_key(node_id, claimed_key);
        return false;
    }
    
    // Check if key matches any known key for this node
    for (const auto& record : it->second) {
        if (record.public_key == claimed_key) {
            return false;  // Matches known key
        }
    }
    
    // Different key detected - potential fork!
    CASHEW_LOG_ERROR("Identity fork detected for node!");
    mark_as_forked(node_id, "Multiple public keys detected");
    return true;
}

bool ForkDetector::verify_signature_consistency(
    const NodeID& node_id,
    const std::vector<uint8_t>& message,
    const Signature& signature
) {
    auto it = node_keys_.find(node_id);
    
    if (it == node_keys_.end()) {
        // No known key, accept but log
        CASHEW_LOG_WARN("No known key for node, cannot verify consistency");
        return true;  // Optimistically accept
    }
    
    // Try to verify with all known keys
    for (const auto& record : it->second) {
        // TODO: Actual signature verification with Ed25519
        // For now, assume valid if we have the key
        (void)record;
        (void)message;
        (void)signature;
        return true;
    }
    
    return false;
}

void ForkDetector::record_node_key(const NodeID& node_id, const PublicKey& public_key) {
    uint64_t current = current_timestamp();
    
    KeyRecord record;
    record.public_key = public_key;
    record.first_seen_time = current;
    record.last_seen_time = current;
    record.signature_count = 0;
    
    node_keys_[node_id].push_back(record);
    
    CASHEW_LOG_DEBUG("Recorded key for node");
}

std::vector<NodeID> ForkDetector::get_detected_forks() const {
    return std::vector<NodeID>(forked_nodes_.begin(), forked_nodes_.end());
}

void ForkDetector::mark_as_forked(const NodeID& node_id, const std::string& reason) {
    forked_nodes_.insert(node_id);
    CASHEW_LOG_ERROR("Marked node as forked: {}", reason);
}

uint64_t ForkDetector::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

// AttackPreventionCoordinator methods

AttackPreventionCoordinator::AttackPreventionCoordinator(
    ledger::Ledger& ledger,
    reputation::ReputationManager& reputation_manager
)
    : ledger_(ledger),
      reputation_manager_(reputation_manager),
      rate_limiter_(),
      sybil_defense_(),
      ddos_mitigation_(),
      fork_detector_(ledger),
      sybil_defense_enabled_(true),
      ddos_mitigation_enabled_(true),
      fork_detection_enabled_(true),
      last_cleanup_time_(current_timestamp())
{
    CASHEW_LOG_INFO("AttackPreventionCoordinator initialized");
}

bool AttackPreventionCoordinator::validate_incoming_connection(
    const std::string& ip_address,
    const NodeID& node_id
) {
    // DDoS check
    if (ddos_mitigation_enabled_ && !ddos_mitigation_.allow_connection(ip_address)) {
        return false;
    }
    
    // Fork check
    if (fork_detection_enabled_) {
        auto forked_nodes = fork_detector_.get_detected_forks();
        if (std::find(forked_nodes.begin(), forked_nodes.end(), node_id) != forked_nodes.end()) {
            CASHEW_LOG_WARN("Rejected connection from forked node");
            return false;
        }
    }
    
    return true;
}

void AttackPreventionCoordinator::on_connection_established(
    const std::string& ip_address,
    const NodeID& node_id
) {
    if (ddos_mitigation_enabled_) {
        ddos_mitigation_.record_connection(ip_address);
    }
    
    (void)node_id;
}

void AttackPreventionCoordinator::on_connection_closed(
    const std::string& ip_address,
    const NodeID& node_id
) {
    if (ddos_mitigation_enabled_) {
        ddos_mitigation_.close_connection(ip_address);
    }
    
    (void)node_id;
}

bool AttackPreventionCoordinator::validate_request(
    const NodeID& node_id,
    const std::string& request_type
) {
    // Rate limit check
    if (!rate_limiter_.allow_request(node_id)) {
        return false;
    }
    
    // Record activity for Sybil defense
    if (sybil_defense_enabled_) {
        sybil_defense_.record_node_activity(node_id, request_type);
    }
    
    return true;
}

bool AttackPreventionCoordinator::validate_new_identity(
    const NodeID& node_id,
    const Hash256& pow_proof
) {
    if (!sybil_defense_enabled_) {
        return true;
    }
    
    return sybil_defense_.validate_new_identity(node_id, pow_proof);
}

bool AttackPreventionCoordinator::validate_signature(
    const NodeID& node_id,
    const std::vector<uint8_t>& message,
    const Signature& signature
) {
    if (!fork_detection_enabled_) {
        return true;
    }
    
    return fork_detector_.verify_signature_consistency(node_id, message, signature);
}

bool AttackPreventionCoordinator::is_under_attack() const {
    if (ddos_mitigation_enabled_) {
        return ddos_mitigation_.detect_attack_pattern();
    }
    return false;
}

float AttackPreventionCoordinator::get_overall_threat_level() const {
    float threat = 0.0f;
    
    if (ddos_mitigation_enabled_) {
        threat = std::max(threat, ddos_mitigation_.get_threat_level());
    }
    
    return threat;
}

void AttackPreventionCoordinator::tick() {
    uint64_t current = current_timestamp();
    
    // Cleanup every 5 minutes
    if (current - last_cleanup_time_ >= 300) {
        cleanup();
        last_cleanup_time_ = current;
    }
    
    // Check for attacks
    if (is_under_attack()) {
        CASHEW_LOG_WARN("DDoS attack pattern detected! Threat level: {:.2f}",
                       get_overall_threat_level());
    }
}

void AttackPreventionCoordinator::cleanup() {
    rate_limiter_.cleanup_stale_entries();
    ddos_mitigation_.cleanup_expired_blocks();
    
    CASHEW_LOG_DEBUG("Attack prevention cleanup completed");
}

AttackPreventionCoordinator::Statistics AttackPreventionCoordinator::get_statistics() const {
    Statistics stats;
    stats.total_connections = ddos_mitigation_.get_total_connections();
    stats.blocked_connections = ddos_mitigation_.get_blocked_connections();
    stats.total_requests = rate_limiter_.get_total_requests();
    stats.blocked_requests = rate_limiter_.get_blocked_requests();
    stats.blocked_ips = ddos_mitigation_.get_blocked_ips_count();
    stats.detected_forks = fork_detector_.get_fork_count();
    stats.threat_level = get_overall_threat_level();
    
    return stats;
}

std::string AttackPreventionCoordinator::Statistics::to_string() const {
    std::ostringstream oss;
    oss << "Attack Prevention Statistics:\n";
    oss << "  Total connections: " << total_connections << "\n";
    oss << "  Blocked connections: " << blocked_connections << "\n";
    oss << "  Total requests: " << total_requests << "\n";
    oss << "  Blocked requests: " << blocked_requests << "\n";
    oss << "  Blocked IPs: " << blocked_ips << "\n";
    oss << "  Detected forks: " << detected_forks << "\n";
    oss << "  Threat level: " << std::fixed << std::setprecision(2) << threat_level;
    return oss.str();
}

void AttackPreventionCoordinator::set_rate_limit_policy(const RateLimitPolicy& policy) {
    rate_limiter_ = RateLimiter(policy);
}

uint64_t AttackPreventionCoordinator::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t);
}

} // namespace cashew::security
