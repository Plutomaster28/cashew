# 🥜 Cashew Network

**A decentralized, self-hosted P2P network where participation does not equal exposure, hosting does not equal vulnerability, and trust is enforced by cryptography instead of infrastructure.**

<div align="center">

![Cashew Logo](docs/assets/logo.png)
*Featuring Mona, our friendly 2channel cat mascot*

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![C++20](https://img.shields.io/badge/C++-20-blue)]()
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey)]()

</div>

---

## 📖 Overview

Cashew is a next-generation decentralized network protocol that combines:
- **Bitcoin's trust model** - trustless, no privileged authority
- **Tor's routing philosophy** - onion-style encrypted routing
- **IPFS content addressing** - hash-based content discovery
- **Zero-trust security** - capability-based access control
- **Fair participation** - adaptive PoW for all hardware levels

### Core Principles

- 🔓 **Freedom over profit** - No subscriptions, no paywalls, no monetization
- 🔒 **Privacy over surveillance** - Pseudonymous identities, no KYC
- ⚖️ **Participation over wealth** - PoW + PoStake, not money
- 🌐 **Protocol over platform** - Decentralized, no monopolies

---

## ✨ Features

### 🛡️ Security & Privacy
- **No exposed services** - All connections outbound-initiated
- **Encrypted everything** - ChaCha20-Poly1305 + forward secrecy
- **Content-addressed** - Identity ≠ location
- **Capability tokens** - Granular, time-limited permissions
- **Attack-resistant** - Anti-Sybil, anti-DDoS, fork detection

### 🌍 Decentralized Architecture
- **P2P mesh networking** - No central servers
- **Gossip protocol** - Decentralized state propagation
- **Multi-hop routing** - Optional onion routing for anonymity
- **Redundant hosting** - Networks provide fault tolerance

### 🔑 Fair Participation
- **Adaptive PoW** - Fair for Raspberry Pi, GPU/ASIC resistant
- **PoStake** - Contribution-based rewards (non-financial)
- **Social vouching** - Trust-based network membership
- **Decay system** - Use it or lose it, prevents hoarding

### 📦 Content System
- **Things** - Content items (games, apps, datasets) up to 500MB
- **Networks** - Invitation-only clusters for redundancy
- **Content integrity** - BLAKE3 hashing, cryptographic verification
- **Gateway access** - Browser access via HTTPS (view-only)

---

## 🎮 First Use Case: Unblocked Games

Cashew's initial deployment focuses on hosting lightweight games through a decentralized network:

- 🕹️ **Host games** on P2P mesh (Tetris, Snake, Pong, etc.)
- 🌐 **Access via browser** through gateway nodes
- 🔒 **Censorship-resistant** - No central host to block
- ⚡ **Fault-tolerant** - Redundant hosting across 3-5 nodes

**Try it:** Just open your browser and play - no registration, no install!

---

## 🏗️ Architecture

```
┌─────────────────────────────────────┐
│       Service Layer                 │  <- Things, Networks, Apps
├─────────────────────────────────────┤
│       Access Layer                  │  <- Capability Tokens
├─────────────────────────────────────┤
│       Discovery Layer               │  <- Gossip, Content Resolution
├─────────────────────────────────────┤
│       Routing Layer                 │  <- Multi-hop, Content-addressed
├─────────────────────────────────────┤
│       Transport Layer               │  <- Encrypted Sessions
├─────────────────────────────────────┤
│       Identity Layer                │  <- Cryptographic Identity
└─────────────────────────────────────┘
```

### Key Components

- **Nodes** - General participants (host, route, earn keys)
- **Things** - Content items (500MB max)
- **Networks** - Redundant hosting clusters (invitation-only)
- **Keys** - Participation rights (PoW/PoStake earned)
- **Gateway** - Web access bridge (HTTPS/WebSocket)

See [PROTOCOL_SPEC.md](PROTOCOL_SPEC.md) for detailed protocol documentation.

---

## 🚀 Getting Started

### Prerequisites

**Linux:**
```bash
sudo apt-get install cmake ninja-build libsodium-dev build-essential
```

**Windows:**
```powershell
# Install CMake, Ninja, and Visual Studio 2022
# Use vcpkg for dependencies
vcpkg install libsodium
```

### Building

```bash
# Clone the repository
git clone https://github.com/yourusername/cashew.git
cd cashew

# Create build directory
mkdir build && cd build

# Configure
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..

# Build
ninja

# Run tests
ctest --verbose

# Run node
./cashew_node
```

### Quick Start

1. **Run a node:**
   ```bash
   ./cashew_node --config node.conf
   ```

2. **Host a Thing (e.g., a game):**
   ```bash
   ./cashew_cli host-thing ./games/tetris.zip
   ```

3. **Access via browser:**
   ```
   http://localhost:7777
   ```

---

## 📚 Documentation

- [Development Plan](DEVELOPMENT_PLAN.md) - Complete roadmap
- [Protocol Specification](PROTOCOL_SPEC.md) - Technical protocol details
- [Project Structure](PROJECT_STRUCTURE.md) - Codebase organization
- [Architecture Guide](docs/architecture.md) - System design
- [API Reference](docs/api_reference.md) - Developer API docs
- [Setup Guide](docs/setup_guide.md) - Installation and configuration

---

## 🛠️ Development

### Project Structure

```
cashew/
├── src/              # Source code
│   ├── core/        # Core protocol (node, thing, network, keys, PoW)
│   ├── crypto/      # Cryptography (Ed25519, X25519, ChaCha20, BLAKE3)
│   ├── network/     # P2P (transport, routing, gossip, discovery)
│   ├── storage/     # Content-addressed storage
│   ├── gateway/     # Web gateway (HTTPS, WebSocket)
│   ├── security/    # Attack prevention, anonymity
│   └── utils/       # Utilities (logging, config)
├── include/         # Public headers
├── tests/           # Unit and integration tests
├── examples/        # Example applications
├── web/             # Web interface (HTML/CSS/JS)
└── docs/            # Documentation
```

### Building Tests

```bash
cmake -G Ninja -DCASHEW_BUILD_TESTS=ON ..
ninja
ctest --verbose
```

### Development Phases

1. **Phase 1**: Foundation (crypto, identity, node)
2. **Phase 2**: Core protocol (Thing, Network, PoW)
3. **Phase 3**: Networking (P2P, routing, gossip)
4. **Phase 4**: State & reputation (ledger, trust)
5. **Phase 5**: Security & privacy (onion routing, tokens)
6. **Phase 6**: Gateway & web layer
7. **Phase 7**: Testing & optimization
8. **Phase 8**: Alpha release (unblocked games)

---

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Areas for Contribution

- 🔐 Cryptography implementation
- 🌐 Networking protocols
- 🎨 Web UI/UX design
- 📝 Documentation
- 🧪 Testing and benchmarking
- 🎮 Game hosting and content

---

## 🎨 Branding

**Name:** Cashew  
**Mascot:** Mona (2channel cat)  
**Colors:** Warm nutty tones (beige, light brown, soft yellow)  
**Vibe:** Friendly, playful, casual, unserious  
**Tagline:** *"Cashew — it just works."*

---

## 🔒 Security

Cashew is designed with security as a first-class concern:

- ✅ No open ports or exposed services
- ✅ Encrypted end-to-end transport
- ✅ Content integrity via cryptographic hashing
- ✅ Attack detection (Sybil, DDoS, identity forks)
- ✅ Capability-based access control
- ✅ Optional anonymity via onion routing

**Responsible Disclosure:** Please report security issues to [security@cashew.network](mailto:security@cashew.network)

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

Inspired by:
- **Bitcoin** - Trustless decentralized consensus
- **Tor** - Anonymous routing
- **IPFS** - Content-addressed storage
- **Libp2p** - P2P networking primitives

Built with love by the Cashew community 🥜🐱

---

## 📞 Contact

- **Website:** [cashew.network](https://cashew.network) *(coming soon)*
- **GitHub:** [github.com/cashew-network/cashew](https://github.com/cashew-network/cashew)
- **Discord:** [discord.gg/cashew](https://discord.gg/cashew) *(coming soon)*
- **Email:** [hello@cashew.network](mailto:hello@cashew.network)

---

<div align="center">

**Not everything needs to be serious. 🥜**

*Cashew: play stupid games, build serious infrastructure.*

</div>
