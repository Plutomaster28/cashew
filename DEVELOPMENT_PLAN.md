# Cashew Network - Complete Development Plan

## Project Overview
**Cashew** is a decentralized, self-hosted P2P network where participation does not equal exposure, hosting does not equal vulnerability, and trust is enforced by cryptography instead of infrastructure.

**Mascot:** Mona (2channel cat)  
**Philosophy:** Freedom over profit, privacy over surveillance, participation over wealth  
**Platform:** Cross-platform (Windows/Linux) using C++, CMake + Ninja  
**First Use Case:** Unblocked games network

---

## 1. Core Components to Build

### 1.1 Node System
**Description:** Individual participant machines in the Cashew network

**Requirements:**
- Cryptographic node identity (Ed25519 public/private key pair)
- Key management (PoW/PoStake/vouched keys)
- Reputation and participation history tracking
- Storage and bandwidth capability management
- Thing hosting capabilities
- Network membership management

**Implementation Tasks:**
- [ ] Node identity generation and management
- [ ] Local key storage with encryption (AES-256)
- [ ] Optional hardware-backed key storage (TPM/Secure Enclave)
- [ ] Node configuration system
- [ ] Node lifecycle management (startup, shutdown, recovery)
- [ ] Reputation score calculator
- [ ] Storage/bandwidth capacity monitor

---

### 1.2 Thing System
**Description:** Content items (games, dictionaries, datasets, apps) with max 500MB size

**Requirements:**
- Content-addressed storage (hash-based)
- Size limit enforcement (500MB max)
- Integrity verification via cryptographic hashing
- Decay mechanism for inactive Things
- Replication coordination

**Implementation Tasks:**
- [ ] Thing creation and validation
- [ ] Content hashing (BLAKE3/SHA-256)
- [ ] Merkle tree/DAG implementation for verification
- [ ] Thing metadata structure
- [ ] Storage backend interface
- [ ] Decay tracking and cleanup
- [ ] Thing serialization/deserialization

---

### 1.3 Network System
**Description:** Specialized node clusters hosting single Things for redundancy

**Requirements:**
- Invitation-only membership
- Acceptance workflow
- Redundancy quorum management (min/max nodes)
- Automatic replication
- Dynamic scaling
- Integrity verification across replicas

**Implementation Tasks:**
- [ ] Network formation protocol
- [ ] Invitation/acceptance workflow
- [ ] Membership verification
- [ ] Replication coordinator
- [ ] Quorum manager
- [ ] Network health monitoring
- [ ] Dynamic redundancy adjustment
- [ ] Network dissolution logic

---

### 1.4 Identity System

#### Machine Identity (Node Identity)
**Requirements:**
- Ed25519 key pair generation
- Key persistence and loading
- Signing and verification
- Key rotation support

**Implementation Tasks:**
- [ ] Ed25519 key generation
- [ ] Secure key storage
- [ ] Message signing interface
- [ ] Signature verification
- [ ] Key rotation mechanism
- [ ] Key revocation protocol

#### Human Identity (Pseudonymous)
**Requirements:**
- Optional pseudonymous identity
- Continuity tracking
- Reputation management
- Social attestation support
- No real-world linking

**Implementation Tasks:**
- [ ] Human identity key generation
- [ ] Reputation calculator
- [ ] Attestation system
- [ ] Trust graph implementation
- [ ] Identity continuity verification
- [ ] Anti-impersonation detection

---

### 1.5 Key System (Participation Keys)
**Description:** Cryptographic tokens granting participation rights (NOT currency)

**Requirements:**
- Uncapped total issuance
- Multiple key types (Identity, Node, Network, Service, Routing)
- PoW-based issuance
- PoStake-based issuance
- Social vouching
- Per-epoch/per-node limits
- Decay and rotation

**Implementation Tasks:**
- [ ] Key type definitions
- [ ] Key generation and validation
- [ ] Key storage and retrieval
- [ ] Issuance tracking
- [ ] Decay scheduler
- [ ] Per-epoch limit enforcement
- [ ] Transfer/vouching system (with anti-abuse)
- [ ] Key revocation

---

### 1.6 Proof-of-Work (PoW) System
**Description:** Adaptive, memory-hard, hardware-normalized effort system

**Requirements:**
- Memory-hard puzzles (Argon2id-style)
- Adaptive difficulty per node
- Network entropy integration
- Time-normalized (target ~10 min per solution)
- Raspberry Pi-friendly
- GPU/ASIC resistant

**Implementation Tasks:**
- [ ] **Adaptive difficulty calculator** (scales per node capability)
- [ ] **Memory-hard puzzle generator** (Argon2id variant)
- [ ] **Node benchmarking system** (measure CPU, memory, latency)
- [ ] **Epoch manager** (10-minute cycles)
- [ ] **Network entropy collector** (gossip state + routing state + time)
- [ ] **Puzzle solver**
- [ ] **Solution verifier**
- [ ] **Reward distribution** (key credit issuance)
- [ ] **Anti-spam limiter** (cap attempts per epoch)

---

### 1.7 Proof-of-Stake (PoStake) System
**Description:** Contribution-based key earning (non-financial)

**Requirements:**
- Uptime tracking
- Bandwidth contribution measurement
- Hosting contribution tracking
- Routing participation tracking
- Service provision tracking
- Reputation integration

**Implementation Tasks:**
- [ ] Uptime monitor
- [ ] Bandwidth usage tracker
- [ ] Contribution score calculator
- [ ] PoStake reward distributor
- [ ] Hybrid PoW/PoStake coordinator

---

### 1.8 Ledger & State Management
**Description:** Distributed ledger tracking network state

**Requirements:**
- Node identities and keys
- Network memberships
- Thing replication status
- Human identities and reputation
- Key issuance/decay logs
- Gossip-based distribution
- No blockchain (event ledger only)

**Implementation Tasks:**
- [ ] Event log structure
- [ ] Append-only log implementation
- [ ] Gossip replication protocol
- [ ] State verification
- [ ] Conflict detection
- [ ] Fork detection
- [ ] Consensus mechanism (lightweight)
- [ ] Query interface

---

### 1.9 Decay System
**Description:** Balance and cleanup mechanism

**Requirements:**
- Inactive node detection
- Unused key expiration
- Thing redundancy adjustment
- Epoch-based checks
- Gradual decay curves

**Implementation Tasks:**
- [ ] Activity monitor
- [ ] Decay scheduler
- [ ] Key expiration logic
- [ ] Thing cleanup coordinator
- [ ] Decay threshold configuration

---

### 1.10 Trust & Reputation System
**Description:** Social verification without real-world identity

**Requirements:**
- Reputation scoring
- Attestation management
- Trust graph construction
- Vouching system
- Reputation decay for inactivity

**Implementation Tasks:**
- [ ] Reputation score algorithm
- [ ] Attestation signing and verification
- [ ] Trust graph data structure
- [ ] Graph traversal for trust calculation
- [ ] Vouching workflow
- [ ] Reputation decay logic

---

## 2. Cryptography Stack

### 2.1 Core Crypto Primitives
**Libraries to use:** libsodium, OpenSSL, or similar

**Requirements:**
- Ed25519 (signatures)
- X25519 (key exchange)
- ChaCha20-Poly1305 (encryption)
- AES-GCM (alternative encryption)
- BLAKE3 (hashing)
- SHA-256 (alternative hashing)
- Argon2id (PoW puzzles)

**Implementation Tasks:**
- [ ] Crypto library integration
- [ ] Wrapper classes for Ed25519
- [ ] Wrapper classes for X25519
- [ ] Symmetric encryption interface
- [ ] Hashing interface
- [ ] Argon2 integration for PoW
- [ ] Random number generation (CSPRNG)
- [ ] Key derivation functions

### 2.2 Encrypted Transport
**Requirements:**
- Session-based encryption
- Forward secrecy
- Ephemeral keys
- Noise Protocol Framework or similar
- QUIC-style sessions

**Implementation Tasks:**
- [ ] Handshake protocol (ECDH-based)
- [ ] Session key generation
- [ ] Session establishment
- [ ] Session teardown
- [ ] Forward secrecy enforcement
- [ ] Replay attack prevention

---

## 3. Network Layer

### 3.1 P2P Mesh Networking
**Requirements:**
- Peer discovery (bootstrap + gossip)
- Dynamic peer selection
- Encrypted peer connections
- Multi-hop routing
- NAT traversal
- IPv4 and IPv6 support

**Implementation Tasks:**
- [ ] Peer discovery protocol
- [ ] Bootstrap node list
- [ ] Peer connection manager
- [ ] Connection pooling
- [ ] NAT traversal (STUN/TURN-like)
- [ ] Routing table management
- [ ] Multi-hop routing implementation

### 3.2 Routing System
**Requirements:**
- Onion-style routing (optional)
- Content-addressed routing
- Session-based paths
- Ephemeral routes
- No static routing tables
- Trustless forwarding

**Implementation Tasks:**
- [ ] Routing protocol design
- [ ] Multi-hop path selection
- [ ] Onion routing implementation
- [ ] Content routing (hash-based)
- [ ] Path encryption layers
- [ ] Route discovery
- [ ] Route maintenance

### 3.3 Gossip Protocol
**Requirements:**
- Periodic state updates
- Network topology distribution
- Entropy propagation (for PoW)
- Trust/reputation propagation
- Key revocation propagation
- Conflict/fork detection

**Implementation Tasks:**
- [ ] Gossip message format
- [ ] Gossip scheduler
- [ ] Message propagation algorithm
- [ ] Anti-spam gossip filtering
- [ ] State reconciliation
- [ ] Bandwidth-efficient gossip

### 3.4 Discovery Layer
**Requirements:**
- DHT-like discovery (optional)
- Gossip-based discovery
- Content hash resolution
- Node capability advertisement

**Implementation Tasks:**
- [ ] Discovery protocol
- [ ] Content lookup by hash
- [ ] Node capability database
- [ ] Discovery cache

---

## 4. Security Layer

### 4.1 Attack Prevention
**Implementation Tasks:**
- [ ] Sybil attack prevention (key gating)
- [ ] DDoS resistance (rate limiting, redundancy)
- [ ] Key theft detection
- [ ] Identity fork detection
- [ ] Content tampering detection
- [ ] Man-in-the-middle prevention

### 4.2 Anonymity & Privacy
**Implementation Tasks:**
- [ ] IP obfuscation (onion routing)
- [ ] Traffic mixing
- [ ] Dynamic peer rotation
- [ ] Ephemeral addressing
- [ ] Metadata minimization

### 4.3 Capability-Based Access
**Requirements:**
- Signed capability tokens
- Time-limited tokens
- Scoped permissions
- Revocable tokens

**Implementation Tasks:**
- [ ] Capability token format
- [ ] Token signing and verification
- [ ] Token scope enforcement
- [ ] Token expiration
- [ ] Token revocation list

---

## 5. Gateway System

### 5.1 Web Gateway (HTTPS Bridge)
**Description:** Regular nodes exposing web interfaces for browser access

**Requirements:**
- HTTPS server
- Mesh-to-web translation
- Read-only browser access
- Session management
- Sandboxed execution
- WebSocket/streaming support

**Implementation Tasks:**
- [ ] HTTP/HTTPS server (embedded or library)
- [ ] WebSocket support
- [ ] Session authentication
- [ ] Mesh query interface
- [ ] Content streaming
- [ ] Browser security (CSP, sandboxing)
- [ ] Gateway API design

### 5.2 Web Application Layer
**Description:** Classic web stack for accessing Things

**Technologies:**
- HTML/CSS/JavaScript
- Optional: PHP, WebAssembly
- WebSockets for real-time
- REST-like APIs

**Implementation Tasks:**
- [ ] Web UI framework selection
- [ ] Gateway API client (JS)
- [ ] Game/content renderer
- [ ] Forum/community interface
- [ ] Mona mascot integration (branding)

---

## 6. Storage Layer

### 6.1 Content-Addressed Storage
**Requirements:**
- Hash-based storage
- Chunking for large Things
- Deduplication
- Integrity verification

**Implementation Tasks:**
- [ ] Storage backend (file system or embedded DB)
- [ ] Content chunking algorithm
- [ ] Hash-to-path mapping
- [ ] Deduplication logic
- [ ] Storage quota management

### 6.2 Metadata Store
**Requirements:**
- Node state
- Thing metadata
- Network membership
- Key holdings
- Reputation data

**Implementation Tasks:**
- [ ] Embedded database (SQLite, LevelDB, RocksDB)
- [ ] Schema design
- [ ] Query interface
- [ ] Backup/restore

---

## 7. Protocol Layers (Detailed)

### 7.1 Identity Layer
- Cryptographic identity
- No IP binding
- No hostname binding
- Key management
- Identity verification

### 7.2 Transport Layer
- Encrypted tunnels
- Ephemeral sessions
- Forward secrecy
- Handshake protocol

### 7.3 Routing Layer
- Onion routing
- Multi-hop paths
- Trustless forwarding
- Content-addressed routing

### 7.4 Discovery Layer
- DHT (optional)
- Gossip protocol
- Content hash resolution
- Node capability discovery

### 7.5 Access Layer
- Capability tokens
- Signed access rights
- Time-bound permissions
- Scope enforcement

### 7.6 Service Layer
- Sandboxed execution
- Isolated services
- Non-persistent endpoints
- Thing hosting

---

## 8. Build System & Project Structure

### 8.1 Build System
**Platform:** Cross-platform (Windows + Linux)  
**Language:** C++ (primary), Rust (optional for specific modules)  
**Build Tool:** CMake + Ninja

**Requirements:**
- Modular CMakeLists.txt
- Cross-compilation support
- Dependency management (vcpkg or Conan)
- Unit testing integration (Catch2 or Google Test)
- Benchmarking support

**Implementation Tasks:**
- [ ] Root CMakeLists.txt
- [ ] Module-based subdirectories
- [ ] Dependency configuration
- [ ] Cross-platform compiler flags
- [ ] Test target configuration
- [ ] Install target configuration

### 8.2 Project Structure
```
cashew/
├── CMakeLists.txt           # Root build config
├── cmake/                   # CMake modules
├── src/
│   ├── core/               # Core protocol implementation
│   │   ├── node/           # Node system
│   │   ├── thing/          # Thing system
│   │   ├── network/        # Network (cluster) system
│   │   ├── identity/       # Identity management
│   │   ├── keys/           # Key system
│   │   ├── pow/            # Proof-of-Work
│   │   ├── postake/        # Proof-of-Stake
│   │   ├── ledger/         # State management
│   │   ├── decay/          # Decay system
│   │   └── reputation/     # Trust & reputation
│   ├── crypto/             # Cryptography primitives
│   ├── network/            # P2P networking
│   │   ├── transport/      # Encrypted transport
│   │   ├── routing/        # Routing protocol
│   │   ├── gossip/         # Gossip protocol
│   │   └── discovery/      # Discovery layer
│   ├── storage/            # Content-addressed storage
│   ├── gateway/            # Web gateway (HTTPS)
│   ├── security/           # Attack prevention, anonymity
│   └── utils/              # Utilities, logging, config
├── include/                # Public headers
├── tests/                  # Unit and integration tests
├── benchmarks/             # Performance benchmarks
├── examples/               # Example applications
├── web/                    # Web interface (HTML/CSS/JS)
├── docs/                   # Documentation
├── tools/                  # Build and development tools
└── third_party/            # External dependencies
```

---

## 9. Development Phases

### Phase 1: Foundation (Weeks 1-4)
**Goal:** Core infrastructure and crypto

**Tasks:**
- [ ] Project setup (CMake, directory structure)
- [ ] Crypto library integration
- [ ] Node identity system
- [ ] Key generation and storage
- [ ] Basic networking (peer connections)
- [ ] Logging and configuration

### Phase 2: Core Protocol (Weeks 5-8)
**Goal:** Thing, Network, and PoW systems

**Tasks:**
- [ ] Thing creation and storage
- [ ] Content hashing and verification
- [ ] Network formation protocol
- [ ] Adaptive PoW implementation
- [ ] Node benchmarking
- [ ] Epoch manager

### Phase 3: Networking & Routing (Weeks 9-12)
**Goal:** P2P mesh and routing

**Tasks:**
- [ ] Peer discovery and gossip
- [ ] Multi-hop routing
- [ ] Encrypted transport
- [ ] NAT traversal
- [ ] Content-addressed routing

### Phase 4: State & Reputation (Weeks 13-16)
**Goal:** Ledger, trust, and decay

**Tasks:**
- [ ] Event ledger implementation
- [ ] Trust graph and reputation
- [ ] PoStake system
- [ ] Decay scheduler
- [ ] Gossip-based state sync

### Phase 5: Security & Privacy (Weeks 17-20)
**Goal:** Attack prevention and anonymity

**Tasks:**
- [ ] Onion routing
- [ ] Key theft detection
- [ ] Identity fork detection
- [ ] Capability tokens
- [ ] Rate limiting and DDoS prevention

### Phase 6: Gateway & Web Layer (Weeks 21-24)
**Goal:** Browser access and web UI

**Tasks:**
- [ ] HTTPS gateway server
- [ ] WebSocket streaming
- [ ] Web UI development
- [ ] Game/content rendering
- [ ] Forum interface

### Phase 7: Testing & Optimization (Weeks 25-28)
**Goal:** Robustness and performance

**Tasks:**
- [ ] Unit test coverage
- [ ] Integration tests
- [ ] Load testing (Raspberry Pi + high-end)
- [ ] Security audits
- [ ] Performance optimization

### Phase 8: Alpha Release (Week 29+)
**Goal:** First use case (unblocked games)

**Tasks:**
- [ ] Deploy test network
- [ ] Host sample games
- [ ] User documentation
- [ ] Community feedback
- [ ] Bug fixes and iteration

---

## 10. Key Technical Decisions

### 10.1 PoW Algorithm
**Decision:** Use adaptive memory-hard puzzles (Argon2id variant)  
**Rationale:** Fair for Raspberry Pi, GPU/ASIC resistant, time-normalized

### 10.2 No Global Key Cap
**Decision:** Uncapped total key issuance  
**Rationale:** Avoid financialization, prevent artificial scarcity

### 10.3 Thing Size Limit
**Decision:** 500MB maximum  
**Rationale:** Enable low-power node participation

### 10.4 Network Membership
**Decision:** Invitation-only with acceptance  
**Rationale:** Controlled redundancy, trust-based participation

### 10.5 Browser Access
**Decision:** Free viewing, key-required participation  
**Rationale:** Open access, spam-resistant contribution

---

## 11. Dependencies & Libraries

### Core Dependencies
- **libsodium** or **OpenSSL**: Cryptography
- **Argon2**: PoW puzzles
- **BLAKE3**: Hashing
- **SQLite** / **LevelDB** / **RocksDB**: Metadata storage
- **Boost.Asio** or **libuv**: Async I/O, networking
- **nlohmann/json** or **RapidJSON**: JSON handling
- **spdlog**: Logging
- **Catch2** or **Google Test**: Testing
- **cpp-httplib** or **Boost.Beast**: HTTP/HTTPS gateway
- **WebSocket++** or **uWebSockets**: WebSocket support

### Optional Dependencies
- **libp2p** (if using existing P2P library)
- **libtorrent** (for some DHT/routing concepts)
- **ZeroMQ** or **nanomsg**: Messaging (if needed)

---

## 12. Metrics & Monitoring

### Network Metrics
- Active node count
- Thing replication count
- Network membership size
- Gossip propagation latency
- Routing hop count

### Node Metrics
- Uptime
- Bandwidth usage
- Storage usage
- PoW success rate
- Reputation score

### Security Metrics
- Key revocations
- Identity forks detected
- Attack attempts
- Rate limit triggers

---

## 13. Documentation Needs

### Technical Docs
- [ ] Protocol specification
- [ ] API reference
- [ ] Architecture diagrams
- [ ] Security model
- [ ] Cryptography details

### User Docs
- [ ] Installation guide
- [ ] Node setup tutorial
- [ ] Hosting a Thing guide
- [ ] Joining a Network guide
- [ ] Web access guide

### Developer Docs
- [ ] Contributing guide
- [ ] Code style guide
- [ ] Build instructions
- [ ] Testing guide
- [ ] Module overview

---

## 14. First Use Case: Unblocked Games

### Requirements
- Host lightweight games (~10-50MB each)
- Low-latency access via web gateway
- Simple web UI (play button + game canvas)
- Fault-tolerant (3-5 node redundancy per game)
- Anti-bot (key-required posting, free viewing)

### Example Games
- Tetris clone
- Snake
- Pong
- 2048
- Simple platformers

---

## 15. Branding & UX

### Mascot: Mona
- 2channel cat reference
- Friendly, playful vibe
- Appears in web UI, logs, success messages

### Color Scheme
- Warm nutty tones (beige, light brown, soft yellow)
- Minimalist, unserious aesthetic

### Tagline Ideas
- "Cashew: participate freely, host confidently, play safely."
- "Cashew — it just works."
- "Not everything needs to be serious."

---

## 16. Success Criteria

### Technical
- [ ] Cross-platform build (Windows + Linux)
- [ ] Raspberry Pi capable
- [ ] <1 second Thing lookup latency
- [ ] 99.9% uptime with 3+ node redundancy
- [ ] Successful PoW on Pi in ~10 minutes

### User Experience
- [ ] <5 minute node setup
- [ ] Instant browser game access
- [ ] No registration/login for viewing
- [ ] <10MB client binary

### Security
- [ ] No successful Sybil attacks
- [ ] No key theft in testing
- [ ] No DDoS vulnerabilities
- [ ] Identity forks detected within 1 minute

---

## 17. Future Enhancements (Post-MVP)

- Mobile clients (Android/iOS)
- Rust implementation for critical modules
- WASM-based web node (browser-based light nodes)
- Distributed forums and wikis
- End-to-end encrypted messaging
- Decentralized package manager
- Video/audio streaming Things
- Inter-network bridges
- Governance mechanisms

---

## Summary

**Cashew is a complete decentralized network framework** built on:
- Bitcoin-style trust model
- Tor-style routing
- IPFS-style content addressing
- Zero-trust security
- Participation-gated access (no money)

**Core Principles:**
- Freedom over profit
- Privacy over surveillance  
- Participation over wealth
- Protocol over platform

**Immediate Goal:** Build a Raspberry Pi-capable, cross-platform (Windows/Linux) P2P network using C++ and CMake, starting with unblocked games as the first use case.

---

**Next Steps:**
1. Set up CMake project structure
2. Integrate crypto libraries
3. Implement Node and Identity systems
4. Build adaptive PoW system
5. Develop Thing and Network protocols
6. Iterate toward alpha release
