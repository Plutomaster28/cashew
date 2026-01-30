# Cashew Network - Quick Summary

## What is Cashew?

Cashew is a **decentralized P2P network** where:
- 🔓 **Hosting ≠ Vulnerability** - Run a node without becoming an attack surface
- 🔒 **Participation ≠ Exposure** - Join the network while preserving privacy
- ⚖️ **Access ≠ Control** - View content freely, earn keys to contribute
- 🌐 **Protocol > Platform** - No monopolies, no subscriptions, no SaaS

## Core Components

### 1. **Nodes**
- General participants in the network
- Can host, route, and earn participation keys
- Identified by cryptographic keys (Ed25519)

### 2. **Things**
- Content items (games, apps, datasets)
- Max size: 500MB
- Content-addressed (BLAKE3 hash)
- Decay if not actively hosted

### 3. **Networks**
- Invitation-only clusters for redundancy
- Host a single Thing across 3-20 nodes
- Fault-tolerant, decentralized hosting

### 4. **Keys**
- Participation rights (NOT currency)
- Earned via PoW (adaptive), PoStake (contribution), or vouching
- Different types: Identity, Node, Network, Service, Routing
- Decay if unused

### 5. **Gateway**
- Web access bridge (HTTPS/WebSocket)
- Browsers can view content (read-only)
- No install required for viewers

## How It Works

### Hosting a Game
```
1. Node earns keys via PoW/PoStake
2. Node creates a Thing (e.g., tetris.zip)
3. Node forms a Network with other nodes
4. Network replicates Thing for redundancy
5. Gateway exposes Thing via HTTPS
6. Browser users play via gateway
```

### Viewing Content
```
1. User opens browser → http://gateway:7777
2. Gateway queries mesh for content
3. Content retrieved via encrypted routing
4. Content streamed to browser
5. No registration, no install, no keys needed
```

### Earning Keys
```
PoW (Effort):
- Solve memory-hard puzzles (Argon2)
- Adaptive difficulty (fair for Raspberry Pi)
- Target: 1 puzzle per 10 minutes

PoStake (Contribution):
- Provide uptime
- Provide bandwidth
- Host Things
- Relay traffic
- Build reputation
```

## Protocol Stack

```
┌─────────────────────────────────┐
│   Service Layer                 │  Things, Networks, Apps
├─────────────────────────────────┤
│   Access Layer                  │  Capability Tokens, Permissions
├─────────────────────────────────┤
│   Discovery Layer               │  Gossip, DHT, Content Resolution
├─────────────────────────────────┤
│   Routing Layer                 │  Multi-hop, Content-addressed
├─────────────────────────────────┤
│   Transport Layer               │  Encrypted Sessions, Forward Secrecy
├─────────────────────────────────┤
│   Identity Layer                │  Cryptographic Identity (Ed25519)
└─────────────────────────────────┘
```

## Key Technologies

### Cryptography
- **Ed25519**: Digital signatures
- **X25519**: Key exchange (ECDH)
- **ChaCha20-Poly1305**: Symmetric encryption
- **BLAKE3**: Content hashing
- **Argon2**: Memory-hard PoW puzzles

### Networking
- **P2P Mesh**: Decentralized topology
- **Gossip Protocol**: State propagation
- **Multi-hop Routing**: Content-addressed
- **Onion Routing** (optional): Anonymity

### Storage
- **Content-addressed**: Hash-based lookup
- **LevelDB/RocksDB**: Metadata storage
- **Chunking & Deduplication**: Efficient storage

## Security Features

- ✅ **No exposed services** - All connections outbound
- ✅ **Encrypted transport** - ChaCha20-Poly1305 + forward secrecy
- ✅ **Content integrity** - BLAKE3 verification
- ✅ **Anti-Sybil** - PoW + PoStake + reputation
- ✅ **Anti-DDoS** - Rate limiting + redundancy
- ✅ **Key security** - Encrypted storage + rotation
- ✅ **Fork detection** - Gossip-based monitoring

## Development Status

### Phase 1: Foundation ✓
- [x] Project structure
- [x] CMake build system
- [x] Core type definitions
- [ ] Crypto library integration
- [ ] Node identity system

### Phase 2: Core Protocol (Current)
- [ ] Thing system
- [ ] Storage backend
- [ ] Adaptive PoW
- [ ] Key system
- [ ] Network clusters

### Phase 3-8: Coming Soon
- Networking & routing
- State & reputation
- Security & privacy
- Gateway & web UI
- Testing & optimization
- Alpha release (games)

## Quick Start (When Ready)

### Run a Node
```bash
./cashew_node --config node.conf
```

### Host a Game
```bash
./cashew_cli host-thing ./games/tetris.zip
```

### Access via Browser
```
http://localhost:7777
```

## Files to Read

1. **README.md** - Project overview
2. **DEVELOPMENT_PLAN.md** - Complete roadmap
3. **PROTOCOL_SPEC.md** - Technical protocol details
4. **PROJECT_STRUCTURE.md** - Code organization
5. **IMPLEMENTATION_CHECKLIST.md** - Detailed tasks
6. **notes.txt** - Original design notes

## First Use Case

**Unblocked Games Network**
- Host lightweight games on P2P mesh
- Access via browser (no install)
- Censorship-resistant (no central host)
- Fault-tolerant (redundant hosting)
- Examples: Tetris, Snake, Pong, 2048

## Philosophy

### Freedom-First
- No subscriptions
- No paywalls
- No monetization
- No platform lock-in

### Privacy-First
- Pseudonymous identities
- No KYC/registration
- No real-world linking
- Optional anonymity

### Fairness
- Adaptive PoW (not hardware arms race)
- Contribution-based rewards
- Social vouching
- No artificial scarcity

## Mascot & Branding

**Name:** Cashew 🥜  
**Mascot:** Mona 🐱 (2channel cat)  
**Vibe:** Friendly, playful, casual  
**Tagline:** *"Cashew — it just works."*

## Next Steps

1. ✅ Read all documentation
2. ⬜ Set up development environment
3. ⬜ Integrate crypto libraries
4. ⬜ Implement Node and Identity
5. ⬜ Build PoW system
6. ⬜ Create Thing and Network systems
7. ⬜ Develop P2P networking
8. ⬜ Build gateway for web access
9. ⬜ Test on Raspberry Pi
10. ⬜ Deploy alpha network

---

**🥜 Welcome to Cashew! Let's build decentralized infrastructure. 🐱**

*"Not everything needs to be serious."*
