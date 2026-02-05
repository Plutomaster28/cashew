#include "security/access.hpp"
#include "utils/logger.hpp"
#include "crypto/blake3.hpp"
#include <algorithm>

namespace cashew::security {

// CapabilityToken methods

bool CapabilityToken::is_expired() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    return static_cast<uint64_t>(now_time_t) >= expires_at;
}

bool CapabilityToken::is_valid_for(const Hash256& context_hash) const {
    if (context.empty()) {
        return true;  // No context requirement
    }
    
    // Hash the context and compare
    auto computed_hash = crypto::Blake3::hash(context);
    
    return computed_hash == context_hash;
}

// AccessDecision methods

AccessDecision AccessDecision::allow(const std::string& reason) {
    return AccessDecision{true, reason, std::nullopt};
}

AccessDecision AccessDecision::deny(const std::string& reason) {
    return AccessDecision{false, reason, std::nullopt};
}

// AccessControl methods

AccessControl::AccessControl(ledger::StateManager& state_manager)
    : state_manager_(state_manager)
{
    initialize_default_policies();
    CASHEW_LOG_INFO("AccessControl initialized");
}

void AccessControl::initialize_default_policies() {
    // Free capabilities (no requirements)
    {
        AccessPolicy policy;
        policy.capability = Capability::VIEW_CONTENT;
        policies_[Capability::VIEW_CONTENT] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::DISCOVER_NETWORKS;
        policies_[Capability::DISCOVER_NETWORKS] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::RELAY_TRAFFIC;
        policies_[Capability::RELAY_TRAFFIC] = policy;
    }
    
    // Requires any key (anti-bot)
    {
        AccessPolicy policy;
        policy.capability = Capability::POST_CONTENT;
        policy.required_key_count = 1;  // Any key type, at least 1
        policies_[Capability::POST_CONTENT] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::VOTE_ON_CONTENT;
        policy.required_key_count = 1;
        policies_[Capability::VOTE_ON_CONTENT] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::CREATE_IDENTITY;
        policy.required_key_count = 1;
        policies_[Capability::CREATE_IDENTITY] = policy;
    }
    
    // Requires specific key types
    {
        AccessPolicy policy;
        policy.capability = Capability::HOST_THINGS;
        policy.required_key_type = core::KeyType::SERVICE;
        policy.required_key_count = 1;
        policies_[Capability::HOST_THINGS] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::JOIN_NETWORKS;
        policy.required_key_type = core::KeyType::NETWORK;
        policy.required_key_count = 1;
        policies_[Capability::JOIN_NETWORKS] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::ROUTE_TRAFFIC;
        policy.required_key_type = core::KeyType::ROUTING;
        policy.required_key_count = 1;
        policies_[Capability::ROUTE_TRAFFIC] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::ISSUE_INVITATIONS;
        policy.required_key_type = core::KeyType::NETWORK;
        policy.required_key_count = 1;
        policy.requires_network_membership = true;
        policies_[Capability::ISSUE_INVITATIONS] = policy;
    }
    
    // Requires reputation
    {
        AccessPolicy policy;
        policy.capability = Capability::VOUCH_FOR_NODES;
        policy.min_reputation = 100;  // Reputation threshold
        policy.required_key_count = 1;
        policies_[Capability::VOUCH_FOR_NODES] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::CREATE_NETWORK;
        policy.min_reputation = 50;
        policy.required_key_type = core::KeyType::NETWORK;
        policy.required_key_count = 3;
        policies_[Capability::CREATE_NETWORK] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::MODERATE_CONTENT;
        policy.min_reputation = 75;
        policy.requires_network_membership = true;
        policy.required_role = "FULL";
        policies_[Capability::MODERATE_CONTENT] = policy;
    }
    
    // Admin capabilities (founder only)
    {
        AccessPolicy policy;
        policy.capability = Capability::REVOKE_KEYS;
        policy.requires_network_membership = true;
        policy.required_role = "FOUNDER";
        policies_[Capability::REVOKE_KEYS] = policy;
    }
    
    {
        AccessPolicy policy;
        policy.capability = Capability::DISBAND_NETWORK;
        policy.requires_network_membership = true;
        policy.required_role = "FOUNDER";
        policies_[Capability::DISBAND_NETWORK] = policy;
    }
    
    CASHEW_LOG_DEBUG("Initialized {} default access policies", policies_.size());
}

void AccessControl::set_policy(Capability capability, const AccessPolicy& policy) {
    policies_[capability] = policy;
}

AccessPolicy AccessControl::get_policy(Capability capability) const {
    auto it = policies_.find(capability);
    if (it == policies_.end()) {
        return AccessPolicy{};  // Empty policy (deny all)
    }
    return it->second;
}

AccessDecision AccessControl::check_access(const AccessRequest& request) const {
    auto policy = get_policy(request.capability);
    
    // Check if node is active
    if (!state_manager_.is_node_active(request.requester)) {
        return AccessDecision::deny("Node is not active");
    }
    
    // Check key requirements
    if (!check_key_requirements(request.requester, policy)) {
        return AccessDecision::deny("Insufficient keys");
    }
    
    // Check reputation requirements
    if (!check_reputation_requirements(request.requester, policy)) {
        return AccessDecision::deny("Insufficient reputation");
    }
    
    // Check network requirements
    if (policy.requires_network_membership) {
        if (!request.network_id) {
            return AccessDecision::deny("Network ID required");
        }
        if (!check_network_requirements(request.requester, *request.network_id, policy)) {
            return AccessDecision::deny("Not a member of network or insufficient role");
        }
    }
    
    // Check PoW requirements (for anonymous posting)
    if (policy.requires_pow) {
        if (!request.pow_solution) {
            return AccessDecision::deny("PoW solution required");
        }
        if (!check_pow_requirements(*request.pow_solution, policy)) {
            return AccessDecision::deny("Invalid PoW solution");
        }
    }
    
    return AccessDecision::allow("Access granted");
}

bool AccessControl::can_view_content(const NodeID& node_id) const {
    // Viewing is always free
    (void)node_id;
    return true;
}

bool AccessControl::can_post_content(const NodeID& node_id) const {
    return has_any_keys(node_id);
}

bool AccessControl::can_post_anonymously(const std::vector<uint8_t>& pow_solution) const {
    AccessPolicy policy;
    policy.requires_pow = true;
    policy.pow_difficulty = 20;  // TODO: Make configurable
    return check_pow_requirements(pow_solution, policy);
}

bool AccessControl::can_host_things(const NodeID& node_id) const {
    return state_manager_.can_node_host_things(node_id);
}

bool AccessControl::can_join_network(const NodeID& node_id, const Hash256& network_id) const {
    if (!state_manager_.can_node_join_networks(node_id)) {
        return false;
    }
    
    // Check if already a member
    return !state_manager_.is_node_in_network(node_id, network_id);
}

bool AccessControl::can_route_traffic(const NodeID& node_id) const {
    return state_manager_.can_node_route_traffic(node_id);
}

bool AccessControl::can_vouch_for_node(const NodeID& voucher, const NodeID& vouchee) const {
    // Voucher needs high reputation and keys
    int32_t reputation = state_manager_.get_node_reputation(voucher);
    if (reputation < 100) {
        return false;
    }
    
    if (!has_any_keys(voucher)) {
        return false;
    }
    
    // Vouchee must be active
    return state_manager_.is_node_active(vouchee);
}

bool AccessControl::can_create_network(const NodeID& node_id) const {
    auto policy = get_policy(Capability::CREATE_NETWORK);
    
    if (!check_key_requirements(node_id, policy)) {
        return false;
    }
    
    return check_reputation_requirements(node_id, policy);
}

bool AccessControl::can_issue_invitation(const NodeID& node_id, const Hash256& network_id) const {
    auto policy = get_policy(Capability::ISSUE_INVITATIONS);
    
    if (!check_key_requirements(node_id, policy)) {
        return false;
    }
    
    return check_network_requirements(node_id, network_id, policy);
}

bool AccessControl::can_moderate_content(const NodeID& node_id, const Hash256& network_id) const {
    auto policy = get_policy(Capability::MODERATE_CONTENT);
    
    if (!check_reputation_requirements(node_id, policy)) {
        return false;
    }
    
    return check_network_requirements(node_id, network_id, policy);
}

bool AccessControl::can_revoke_keys(const NodeID& node_id, const Hash256& network_id) const {
    return is_network_founder(node_id, network_id);
}

bool AccessControl::can_disband_network(const NodeID& node_id, const Hash256& network_id) const {
    return is_network_founder(node_id, network_id);
}

std::optional<CapabilityToken> AccessControl::issue_token(
    const NodeID& node_id,
    Capability capability,
    const std::optional<Hash256>& context) const
{
    AccessRequest request;
    request.requester = node_id;
    request.capability = capability;
    if (context) {
        request.network_id = context;
    }
    
    auto decision = check_access(request);
    if (!decision.granted) {
        return std::nullopt;
    }
    
    CapabilityToken token;
    token.node_id = node_id;
    token.capability = capability;
    token.signature = {};  // Initialize empty signature
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    token.issued_at = static_cast<uint64_t>(now_time_t);
    token.expires_at = token.issued_at + 3600;  // 1 hour expiry
    
    if (context) {
        token.context.assign(context->begin(), context->end());
    }
    
    // TODO: Sign token with node's private key
    // For now, leave signature empty (will be filled when we integrate identity)
    
    return token;
}

bool AccessControl::verify_token(const CapabilityToken& token) const {
    if (token.is_expired()) {
        return false;
    }
    
    // TODO: Verify signature
    // For now, just check expiry
    
    return true;
}

AccessLevel AccessControl::get_access_level(const NodeID& node_id) const {
    if (!state_manager_.is_node_active(node_id)) {
        return AccessLevel::ANONYMOUS;
    }
    
    // Check if has identity key
    bool has_identity = state_manager_.get_node_key_balance(node_id, core::KeyType::IDENTITY) > 0;
    if (!has_identity) {
        return AccessLevel::ANONYMOUS;
    }
    
    // Check if has any other keys
    if (!has_any_keys(node_id)) {
        return AccessLevel::IDENTIFIED;
    }
    
    // Check reputation
    int32_t reputation = state_manager_.get_node_reputation(node_id);
    if (reputation >= 100) {
        return AccessLevel::TRUSTED;
    }
    
    return AccessLevel::KEYED;
}

AccessLevel AccessControl::get_access_level_in_network(const NodeID& node_id, 
                                                        const Hash256& network_id) const
{
    if (is_network_founder(node_id, network_id)) {
        return AccessLevel::FOUNDER;
    }
    
    return get_access_level(node_id);
}

size_t AccessControl::count_nodes_with_capability(Capability capability) const {
    size_t count = 0;
    
    auto all_nodes = state_manager_.get_all_active_nodes();
    for (const auto& node_state : all_nodes) {
        AccessRequest request;
        request.requester = node_state.node_id;
        request.capability = capability;
        
        auto decision = check_access(request);
        if (decision.granted) {
            count++;
        }
    }
    
    return count;
}

bool AccessControl::check_key_requirements(const NodeID& node_id, const AccessPolicy& policy) const {
    if (policy.required_key_count == 0) {
        return true;  // No key requirement
    }
    
    if (policy.required_key_type) {
        // Specific key type required
        uint32_t balance = state_manager_.get_node_key_balance(node_id, *policy.required_key_type);
        return balance >= policy.required_key_count;
    } else {
        // Any key type acceptable
        return has_any_keys(node_id);
    }
}

bool AccessControl::check_reputation_requirements(const NodeID& node_id, const AccessPolicy& policy) const {
    if (policy.min_reputation == 0) {
        return true;  // No reputation requirement
    }
    
    int32_t reputation = state_manager_.get_node_reputation(node_id);
    return reputation >= policy.min_reputation;
}

bool AccessControl::check_network_requirements(const NodeID& node_id, 
                                                const Hash256& network_id,
                                                const AccessPolicy& policy) const
{
    if (!policy.requires_network_membership) {
        return true;
    }
    
    if (!state_manager_.is_node_in_network(node_id, network_id)) {
        return false;
    }
    
    if (policy.required_role) {
        auto network_state = state_manager_.get_network_state(network_id);
        if (!network_state) {
            return false;
        }
        
        std::string actual_role = network_state->get_member_role(node_id);
        return actual_role == *policy.required_role;
    }
    
    return true;
}

bool AccessControl::check_pow_requirements(const std::vector<uint8_t>& pow_solution,
                                            const AccessPolicy& policy) const
{
    if (!policy.requires_pow) {
        return true;
    }
    
    // TODO: Verify PoW solution meets difficulty
    // For now, just check it's not empty
    return !pow_solution.empty();
}

bool AccessControl::is_network_founder(const NodeID& node_id, const Hash256& network_id) const {
    auto network_state = state_manager_.get_network_state(network_id);
    if (!network_state) {
        return false;
    }
    
    std::string role = network_state->get_member_role(node_id);
    return role == "FOUNDER";
}

bool AccessControl::has_any_keys(const NodeID& node_id) const {
    auto node_state = state_manager_.get_node_state(node_id);
    if (!node_state) {
        return false;
    }
    
    for (const auto& [key_type, count] : node_state->key_balances) {
        if (count > 0) {
            return true;
        }
    }
    
    return false;
}

} // namespace cashew::security
