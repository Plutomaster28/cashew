# Cashew Protocol Specification v0.1

## Table of Contents
1. [Protocol Stack Overview](#protocol-stack-overview)
2. [Identity Layer](#identity-layer)
3. [Transport Layer](#transport-layer)
4. [Routing Layer](#routing-layer)
5. [Discovery Layer](#discovery-layer)
6. [Access Layer](#access-layer)
7. [Service Layer](#service-layer)
8. [Message Formats](#message-formats)
9. [Security Considerations](#security-considerations)

---

## Protocol Stack Overview

```
┌─────────────────────────────────────┐
│       Service Layer                 │  <- Things, Networks, Applications
├─────────────────────────────────────┤
│       Access Layer                  │  <- Capability Tokens, Permissions
├─────────────────────────────────────┤
│       Discovery Layer               │  <- Gossip, Content Resolution
├─────────────────────────────────────┤
│       Routing Layer                 │  <- Multi-hop, Content-addressed
├─────────────────────────────────────┤
│       Transport Layer               │  <- Encrypted Sessions, Forward Secrecy
├─────────────────────────────────────┤
│       Identity Layer                │  <- Cryptographic Identity, Keys
└─────────────────────────────────────┘
```

---

## Identity Layer

### Purpose
Establish cryptographic identities for nodes and humans without IP/location binding.

### Components

#### Node Identity
```cpp
struct NodeIdentity {
    Ed25519PublicKey public_key;     // 32 bytes
    Ed25519SecretKey secret_key;     // 32 bytes (stored encrypted)
    NodeID node_id;                  // SHA256(public_key)
    uint64_t created_timestamp;
    uint64_t last_rotation_timestamp;
};
```

#### Human Identity (Pseudonymous)
```cpp
struct HumanIdentity {
    Ed25519PublicKey public_key;     // 32 bytes
    Ed25519SecretKey secret_key;     // 32 bytes (stored encrypted)
    HumanID human_id;                // SHA256(public_key)
    uint64_t created_timestamp;
    ReputationScore reputation;
    vector<Attestation> attestations;
};
```

### Operations

#### 1. Identity Generation
```
GENERATE_IDENTITY():
    (public_key, secret_key) = Ed25519_KeyGen()
    id = SHA256(public_key)
    timestamp = current_time()
    return Identity(public_key, secret_key, id, timestamp)
```

#### 2. Identity Verification
```
VERIFY_IDENTITY(message, signature, public_key):
    return Ed25519_Verify(message, signature, public_key)
```

#### 3. Identity Rotation
```
ROTATE_IDENTITY(old_identity, new_public_key):
    rotation_cert = Sign(old_secret_key, new_public_key || timestamp)
    broadcast rotation_cert to gossip network
    return new_identity
```

### Security Properties
- **No Location Binding:** Identity ≠ IP address
- **Key Rotation:** Periodic rotation supported with signed certificates
- **Forward Secrecy:** Old keys can be safely discarded
- **Revocation:** Compromised keys broadcast via gossip

---

## Transport Layer

### Purpose
Establish encrypted, session-based, outbound-only connections between nodes.

### Design Principles
- **No Inbound Connections:** All connections initiated outbound
- **Ephemeral Sessions:** Short-lived, disposable session keys
- **Forward Secrecy:** Session compromise doesn't reveal past sessions
- **Encrypted Tunnels:** All traffic encrypted end-to-end

### Handshake Protocol

#### Phase 1: Connection Initiation
```
Initiator → Responder:
    {
        version: uint8,
        initiator_ephemeral_public: X25519PublicKey,  // 32 bytes
        initiator_node_id: NodeID,                    // 32 bytes
        timestamp: uint64,
        signature: Ed25519Signature                    // 64 bytes
    }
```

#### Phase 2: Response
```
Responder → Initiator:
    {
        responder_ephemeral_public: X25519PublicKey,
        responder_node_id: NodeID,
        timestamp: uint64,
        signature: Ed25519Signature
    }
```

#### Phase 3: Session Key Derivation
```
shared_secret = X25519_DH(initiator_ephemeral_secret, responder_ephemeral_public)
session_key = HKDF_SHA256(shared_secret, "cashew_session_v1")
tx_key = session_key[0:32]    // ChaCha20-Poly1305 key for sending
rx_key = session_key[32:64]   // ChaCha20-Poly1305 key for receiving
```

### Session Management

#### Session Structure
```cpp
struct Session {
    NodeID peer_id;
    X25519PublicKey peer_ephemeral_key;
    ChaCha20Key tx_key;
    ChaCha20Key rx_key;
    uint64_t created_timestamp;
    uint64_t last_activity_timestamp;
    uint64_t messages_sent;
    uint64_t bytes_sent;
};
```

#### Session Lifecycle
1. **Creation:** After successful handshake
2. **Active:** Messages encrypted with session keys
3. **Idle Timeout:** Close after 30 minutes inactivity
4. **Explicit Close:** Either party sends CLOSE message
5. **Rekeying:** New session after 1 hour or 1GB traffic

### Encrypted Messages
```
EncryptedMessage:
    {
        nonce: 12 bytes (random),
        ciphertext: variable length,
        auth_tag: 16 bytes (Poly1305)
    }

ENCRYPT(plaintext, session_key):
    nonce = random(12)
    (ciphertext, tag) = ChaCha20_Poly1305_Encrypt(plaintext, nonce, session_key)
    return EncryptedMessage(nonce, ciphertext, tag)

DECRYPT(encrypted_message, session_key):
    plaintext = ChaCha20_Poly1305_Decrypt(
        encrypted_message.ciphertext,
        encrypted_message.nonce,
        encrypted_message.auth_tag,
        session_key
    )
    return plaintext
```

### Security Properties
- **Forward Secrecy:** Ephemeral keys destroyed after session
- **Replay Protection:** Nonce uniqueness enforced
- **Authentication:** Both parties verified via Ed25519 signatures
- **Confidentiality:** ChaCha20 encryption

---

## Routing Layer

### Purpose
Route content requests and responses through multi-hop paths without revealing source/destination.

### Content-Addressed Routing

#### Content Request
```
ContentRequest:
    {
        content_hash: BLAKE3_Hash,        // 32 bytes
        requester_token: CapabilityToken, // Optional
        hop_limit: uint8,                 // Max hops remaining
        request_id: UUID,                 // 16 bytes
        timestamp: uint64
    }
```

#### Content Response
```
ContentResponse:
    {
        content_hash: BLAKE3_Hash,
        content_data: bytes,
        signature: Ed25519Signature,      // Signed by hosting node
        request_id: UUID,
        hop_count: uint8
    }
```

### Multi-Hop Routing

#### Onion Routing (Optional)
For enhanced anonymity, requests can be wrapped in layers:

```
Layer 3 (Innermost):
    {
        content_hash: BLAKE3_Hash,
        request_id: UUID
    }

Layer 2:
    {
        next_hop: NodeID_2,
        encrypted_payload: Encrypt(Layer3, Key_2)
    }

Layer 1 (Outermost):
    {
        next_hop: NodeID_1,
        encrypted_payload: Encrypt(Layer2, Key_1)
    }
```

Each hop decrypts one layer and forwards to next_hop.

### Routing Table

```cpp
struct RoutingEntry {
    NodeID node_id;
    vector<ContentHash> advertised_content;
    uint8_t hop_distance;
    uint64_t last_seen_timestamp;
    float reliability_score;
};

struct RoutingTable {
    map<NodeID, RoutingEntry> nodes;
    map<ContentHash, vector<NodeID>> content_index;
};
```

### Routing Algorithm

```
ROUTE_CONTENT_REQUEST(content_hash):
    // 1. Check local storage
    if LOCAL_STORAGE.has(content_hash):
        return LOCAL_STORAGE.get(content_hash)
    
    // 2. Check routing table for known hosts
    hosts = ROUTING_TABLE.get_hosts_for(content_hash)
    if hosts.not_empty():
        selected_host = SELECT_BEST_HOST(hosts)
        return FORWARD_REQUEST(content_hash, selected_host)
    
    // 3. Gossip query for unknown content
    return GOSSIP_QUERY(content_hash)
```

### Security Properties
- **Location Privacy:** Content-addressed, not location-addressed
- **Hop Limiting:** Prevents routing loops
- **Trustless:** No single node can censor or monopolize routing
- **Redundancy:** Multiple paths for same content

---

## Discovery Layer

### Purpose
Enable nodes to discover peers, content, and network topology without central servers.

### Bootstrap Discovery

#### Bootstrap Nodes
Hardcoded list of well-known entry nodes:

```
bootstrap_nodes = [
    "node1_public_key@bootstrap1.cashew",
    "node2_public_key@bootstrap2.cashew",
    ...
]
```

#### Bootstrap Process
```
BOOTSTRAP():
    for each bootstrap_node in bootstrap_nodes:
        try:
            session = CONNECT(bootstrap_node)
            peers = REQUEST_PEERS(session, count=20)
            ROUTING_TABLE.add(peers)
            break  // Success
        catch ConnectionError:
            continue  // Try next bootstrap
```

### Gossip Protocol

#### Gossip Message Types

**1. Peer Announcement**
```
PeerAnnouncement:
    {
        node_id: NodeID,
        public_key: Ed25519PublicKey,
        capabilities: NodeCapabilities,
        timestamp: uint64,
        signature: Ed25519Signature
    }
```

**2. Content Announcement**
```
ContentAnnouncement:
    {
        content_hash: BLAKE3_Hash,
        content_size: uint64,
        hosting_node: NodeID,
        network_id: NetworkID (optional),
        timestamp: uint64,
        signature: Ed25519Signature
    }
```

**3. Network State Update**
```
NetworkStateUpdate:
    {
        epoch_number: uint64,
        active_nodes: vector<NodeID>,
        pow_difficulty: uint32,
        entropy_seed: bytes,
        timestamp: uint64,
        signatures: vector<Ed25519Signature>  // Multi-party signed
    }
```

#### Gossip Propagation Algorithm

```
GOSSIP_PROPAGATE(message):
    // 1. Verify message signature
    if not VERIFY_SIGNATURE(message):
        return  // Reject invalid messages
    
    // 2. Check if already seen (deduplication)
    if SEEN_MESSAGES.contains(message.hash()):
        return  // Already propagated
    
    // 3. Add to seen set
    SEEN_MESSAGES.add(message.hash())
    
    // 4. Process message locally
    PROCESS_MESSAGE(message)
    
    // 5. Forward to random subset of peers
    peers = ROUTING_TABLE.get_random_peers(count=3)
    for peer in peers:
        SEND_ASYNC(peer, message)
```

#### Gossip Interval
- **Peer announcements:** Every 5 minutes
- **Content announcements:** On hosting/joining network
- **Network state:** Every epoch (10 minutes)

### DHT-Style Discovery (Optional)

For scalability, a Kademlia-inspired DHT can be used:

```cpp
struct DHTNode {
    NodeID id;
    XOR_Distance distance;  // XOR distance from local node
    uint64_t last_seen;
};

struct DHTBucket {
    vector<DHTNode> nodes;  // Max 20 nodes per bucket
};

struct DHT {
    array<DHTBucket, 256> buckets;  // 256 buckets for 256-bit IDs
};
```

### Security Properties
- **Sybil Resistant:** Gossip combined with PoW/PoStake prevents spam
- **Censorship Resistant:** No central directory
- **Byzantine Tolerant:** Multiple sources for same information

---

## Access Layer

### Purpose
Control access to resources using cryptographic capability tokens.

### Capability Tokens

#### Token Structure
```cpp
struct CapabilityToken {
    TokenID token_id;              // 32 bytes (random)
    NodeID issuer;                 // Who issued this token
    NodeID bearer;                 // Who holds this token (optional for transferable tokens)
    vector<Permission> permissions;
    uint64_t issued_timestamp;
    uint64_t expiry_timestamp;
    uint32_t max_uses;             // 0 = unlimited
    Ed25519Signature issuer_signature;
};
```

#### Permission Types
```cpp
enum Permission {
    READ_CONTENT,          // View/download content
    HOST_CONTENT,          // Host Things
    JOIN_NETWORK,          // Join a Network cluster
    CREATE_IDENTITY,       // Create new pseudonymous identity
    POST_CONTENT,          // Post in forums/create content
    ROUTE_TRAFFIC,         // Participate in routing
    VOTE_GOVERNANCE        // Participate in network governance (future)
};
```

### Token Operations

#### 1. Token Issuance
```
ISSUE_TOKEN(issuer_identity, bearer, permissions, expiry):
    token_id = random(32)
    token = CapabilityToken {
        token_id: token_id,
        issuer: issuer_identity.node_id,
        bearer: bearer,
        permissions: permissions,
        issued_timestamp: now(),
        expiry_timestamp: expiry,
        max_uses: 0
    }
    token.signature = Sign(issuer_identity.secret_key, token)
    return token
```

#### 2. Token Verification
```
VERIFY_TOKEN(token, required_permission):
    // 1. Check signature
    if not VERIFY_SIGNATURE(token):
        return false
    
    // 2. Check expiry
    if token.expiry_timestamp < now():
        return false
    
    // 3. Check permission
    if required_permission not in token.permissions:
        return false
    
    // 4. Check revocation list
    if REVOCATION_LIST.contains(token.token_id):
        return false
    
    return true
```

#### 3. Token Revocation
```
REVOKE_TOKEN(issuer_identity, token_id):
    revocation = {
        token_id: token_id,
        issuer: issuer_identity.node_id,
        timestamp: now(),
        signature: Sign(issuer_identity.secret_key, token_id || timestamp)
    }
    GOSSIP_BROADCAST(revocation)
    REVOCATION_LIST.add(token_id)
```

### Key-Based Access

Participation keys grant implicit capabilities:

```cpp
struct KeyCapabilities {
    KeyType key_type;
    vector<Permission> implicit_permissions;
};

MAP<KeyType, vector<Permission>> KEY_PERMISSIONS = {
    {IDENTITY_KEY, {CREATE_IDENTITY}},
    {NODE_KEY, {HOST_CONTENT, READ_CONTENT}},
    {NETWORK_KEY, {JOIN_NETWORK}},
    {SERVICE_KEY, {POST_CONTENT}},
    {ROUTING_KEY, {ROUTE_TRAFFIC}}
};
```

### Security Properties
- **Least Privilege:** Tokens scoped to specific permissions
- **Revocable:** Tokens can be revoked via gossip
- **Time-Limited:** Expiry timestamps prevent indefinite access
- **Auditable:** All token use logged (optional)

---

## Service Layer

### Purpose
Host and provide Things (content/apps/services) in a sandboxed, decentralized manner.

### Thing Protocol

#### Thing Metadata
```cpp
struct ThingMeta {
    ContentHash content_hash;         // BLAKE3(content)
    string name;
    string description;
    uint64_t size_bytes;              // Max 500MB
    MimeType content_type;
    vector<string> tags;
    NodeID creator;
    uint64_t created_timestamp;
    Ed25519Signature creator_signature;
};
```

#### Thing Hosting
```
HOST_THING(thing_data, thing_meta):
    // 1. Verify size limit
    if thing_meta.size_bytes > 500_MB:
        return error("Thing too large")
    
    // 2. Compute content hash
    computed_hash = BLAKE3(thing_data)
    if computed_hash != thing_meta.content_hash:
        return error("Hash mismatch")
    
    // 3. Verify signature
    if not VERIFY_SIGNATURE(thing_meta):
        return error("Invalid signature")
    
    // 4. Store locally
    STORAGE.put(content_hash, thing_data)
    STORAGE.put_meta(content_hash, thing_meta)
    
    // 5. Announce via gossip
    GOSSIP_BROADCAST(ContentAnnouncement {
        content_hash: content_hash,
        hosting_node: LOCAL_NODE_ID,
        timestamp: now()
    })
```

### Network (Cluster) Protocol

#### Network Formation
```
CREATE_NETWORK(thing_hash, founder_nodes):
    network_id = BLAKE3(thing_hash || founder_nodes || timestamp)
    network = Network {
        network_id: network_id,
        thing_hash: thing_hash,
        members: founder_nodes,
        quorum_min: 3,
        quorum_max: 20,
        created_timestamp: now()
    }
    
    // All founders sign
    for node in founder_nodes:
        signature = Sign(node.secret_key, network)
        network.signatures.append(signature)
    
    GOSSIP_BROADCAST(network)
    return network
```

#### Network Invitation
```
INVITE_TO_NETWORK(network_id, invitee_node_id, inviter_identity):
    invitation = NetworkInvitation {
        network_id: network_id,
        invitee: invitee_node_id,
        inviter: inviter_identity.node_id,
        timestamp: now(),
        signature: Sign(inviter_identity.secret_key, ...)
    }
    
    SEND_MESSAGE(invitee_node_id, invitation)
```

#### Network Acceptance
```
ACCEPT_NETWORK_INVITATION(invitation, acceptor_identity):
    // 1. Verify invitation
    if not VERIFY_SIGNATURE(invitation):
        return error("Invalid invitation")
    
    // 2. Verify inviter is member
    network = LEDGER.get_network(invitation.network_id)
    if invitation.inviter not in network.members:
        return error("Inviter not a member")
    
    // 3. Accept and join
    acceptance = NetworkAcceptance {
        network_id: invitation.network_id,
        acceptor: acceptor_identity.node_id,
        timestamp: now(),
        signature: Sign(acceptor_identity.secret_key, ...)
    }
    
    // 4. Begin replication
    thing_hash = network.thing_hash
    REPLICATE_THING(thing_hash, network.members)
    
    GOSSIP_BROADCAST(acceptance)
```

#### Thing Replication
```
REPLICATE_THING(thing_hash, source_nodes):
    for source in source_nodes:
        try:
            thing_data = REQUEST_CONTENT(source, thing_hash)
            thing_meta = REQUEST_METADATA(source, thing_hash)
            
            // Verify integrity
            if BLAKE3(thing_data) != thing_hash:
                continue  // Try next source
            
            // Store locally
            STORAGE.put(thing_hash, thing_data)
            STORAGE.put_meta(thing_hash, thing_meta)
            return success
        catch:
            continue
    
    return error("Replication failed")
```

### Service Execution (Future)

For dynamic services (not just static Things):

```cpp
struct Service {
    ContentHash service_hash;
    string service_name;
    ServiceType type;  // WASM, Native, Container
    vector<Port> ports;
    vector<Dependency> dependencies;
    ResourceLimits limits;
};
```

Sandboxed execution using WASM or containers.

---

## Message Formats

### Wire Format

All messages use MessagePack or Protocol Buffers for efficient serialization.

#### Generic Message Envelope
```
Message:
    {
        version: uint8,
        type: MessageType,
        payload: bytes,
        timestamp: uint64,
        signature: Ed25519Signature (optional)
    }
```

#### Message Types
```cpp
enum MessageType {
    // Identity Layer
    IDENTITY_ANNOUNCEMENT = 0x01,
    IDENTITY_ROTATION = 0x02,
    
    // Transport Layer
    HANDSHAKE_INIT = 0x10,
    HANDSHAKE_RESPONSE = 0x11,
    SESSION_DATA = 0x12,
    SESSION_CLOSE = 0x13,
    
    // Routing Layer
    CONTENT_REQUEST = 0x20,
    CONTENT_RESPONSE = 0x21,
    ROUTE_ADVERTISEMENT = 0x22,
    
    // Discovery Layer
    PEER_ANNOUNCEMENT = 0x30,
    PEER_REQUEST = 0x31,
    GOSSIP_MESSAGE = 0x32,
    DHT_QUERY = 0x33,
    DHT_RESPONSE = 0x34,
    
    // Access Layer
    CAPABILITY_TOKEN = 0x40,
    TOKEN_REVOCATION = 0x41,
    
    // Service Layer
    THING_METADATA = 0x50,
    THING_DATA = 0x51,
    NETWORK_CREATION = 0x52,
    NETWORK_INVITATION = 0x53,
    NETWORK_ACCEPTANCE = 0x54,
    
    // PoW/PoStake
    POW_CHALLENGE = 0x60,
    POW_SOLUTION = 0x61,
    POSTAKE_REPORT = 0x62,
    KEY_ISSUANCE = 0x63
};
```

---

## Security Considerations

### Attack Vectors & Mitigations

#### 1. Sybil Attack
**Attack:** Attacker creates many fake nodes.  
**Mitigation:**
- PoW/PoStake key gating
- Reputation-based trust
- Vouching requirements for Networks

#### 2. Eclipse Attack
**Attack:** Attacker isolates a node by controlling all its peers.  
**Mitigation:**
- Random peer selection
- Diversity requirements (geographic, autonomous system)
- Bootstrap node fallback

#### 3. DDoS
**Attack:** Flood a node with requests.  
**Mitigation:**
- Rate limiting per peer
- Capability tokens for heavy operations
- Network redundancy (no single point of failure)

#### 4. Key Theft
**Attack:** Steal node's private keys.  
**Mitigation:**
- Encrypted key storage
- Hardware-backed keys (TPM)
- Key rotation
- Challenge-response authentication

#### 5. Content Poisoning
**Attack:** Inject malicious content with false hashes.  
**Mitigation:**
- Content-addressed verification (BLAKE3)
- Multi-source verification in Networks
- Creator signatures

#### 6. Routing Manipulation
**Attack:** Malicious routing to censor or monitor traffic.  
**Mitigation:**
- Multi-hop routing
- Onion-style encryption
- Route diversity
- Reputation-based peer selection

#### 7. Identity Fork
**Attack:** Attacker uses stolen key to impersonate node.  
**Mitigation:**
- Gossip-based fork detection
- Behavioral fingerprinting
- Multi-party attestation

---

## Protocol Versioning

### Version Negotiation
```
NEGOTIATE_VERSION(peer):
    local_version = CURRENT_PROTOCOL_VERSION
    peer_version = PEER_VERSION(peer)
    
    if peer_version > local_version:
        warn("Peer using newer protocol")
        return min(local_version, peer_version)
    
    return local_version
```

### Compatibility
- **Backward compatible:** New versions support old messages
- **Feature flags:** Nodes advertise supported features
- **Graceful degradation:** Fall back to common subset

---

## Conclusion

This protocol specification defines the complete Cashew network stack, from cryptographic identity to service hosting. All layers are designed to be:

- **Decentralized:** No central authority
- **Trustless:** Cryptographic verification, not trust
- **Private:** Identity ≠ location, optional anonymity
- **Secure:** Multiple defenses against known attacks
- **Efficient:** Lightweight enough for Raspberry Pi

Implementation proceeds layer by layer, starting with Identity and Transport, then building up to Discovery, Routing, Access, and finally Service layers.

---

**Next Steps:**
1. Implement Identity Layer in C++
2. Build Transport Layer with encrypted sessions
3. Develop Gossip and Discovery protocols
4. Create Routing layer with content-addressing
5. Build Access layer with capability tokens
6. Implement Service layer for Things and Networks
