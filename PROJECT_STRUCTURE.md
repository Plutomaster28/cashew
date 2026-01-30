# Cashew Project Structure

```
cashew/
├── CMakeLists.txt                 # Root build configuration
├── README.md                       # Project overview
├── LICENSE                         # License file
├── DEVELOPMENT_PLAN.md            # Complete development roadmap
├── PROTOCOL_SPEC.md               # Protocol specification
├── notes.txt                       # Original design notes
│
├── cmake/                          # CMake modules and scripts
│   ├── FindSodium.cmake           # Find libsodium
│   ├── CompilerWarnings.cmake     # Compiler warning flags
│   └── Sanitizers.cmake           # Sanitizer configuration
│
├── include/cashew/                 # Public headers
│   ├── common.hpp                 # Common types and constants
│   ├── version.hpp                # Version information
│   └── cashew.hpp                 # Main include header
│
├── src/                            # Source code
│   ├── CMakeLists.txt             # Source build configuration
│   ├── main.cpp                   # Node executable entry point
│   │
│   ├── core/                      # Core protocol implementation
│   │   ├── CMakeLists.txt
│   │   ├── node/                  # Node system
│   │   │   ├── node.hpp
│   │   │   ├── node.cpp
│   │   │   ├── node_identity.hpp
│   │   │   ├── node_identity.cpp
│   │   │   └── node_config.hpp
│   │   │
│   │   ├── thing/                 # Thing (content) system
│   │   │   ├── thing.hpp
│   │   │   ├── thing.cpp
│   │   │   ├── thing_metadata.hpp
│   │   │   └── thing_manager.hpp
│   │   │
│   │   ├── network/               # Network (cluster) system
│   │   │   ├── network.hpp
│   │   │   ├── network.cpp
│   │   │   ├── network_manager.hpp
│   │   │   ├── invitation.hpp
│   │   │   └── replication.hpp
│   │   │
│   │   ├── identity/              # Identity management
│   │   │   ├── machine_identity.hpp
│   │   │   ├── machine_identity.cpp
│   │   │   ├── human_identity.hpp
│   │   │   ├── human_identity.cpp
│   │   │   └── identity_manager.hpp
│   │   │
│   │   ├── keys/                  # Participation key system
│   │   │   ├── key.hpp
│   │   │   ├── key.cpp
│   │   │   ├── key_types.hpp
│   │   │   ├── key_manager.hpp
│   │   │   └── key_store.hpp
│   │   │
│   │   ├── pow/                   # Proof-of-Work system
│   │   │   ├── pow.hpp
│   │   │   ├── pow.cpp
│   │   │   ├── puzzle.hpp
│   │   │   ├── adaptive_difficulty.hpp
│   │   │   ├── benchmark.hpp
│   │   │   └── solver.hpp
│   │   │
│   │   ├── postake/               # Proof-of-Stake system
│   │   │   ├── postake.hpp
│   │   │   ├── postake.cpp
│   │   │   ├── contribution_tracker.hpp
│   │   │   └── reward_calculator.hpp
│   │   │
│   │   ├── ledger/                # State management & ledger
│   │   │   ├── ledger.hpp
│   │   │   ├── ledger.cpp
│   │   │   ├── event_log.hpp
│   │   │   ├── state_manager.hpp
│   │   │   └── consensus.hpp
│   │   │
│   │   ├── decay/                 # Decay system
│   │   │   ├── decay.hpp
│   │   │   ├── decay.cpp
│   │   │   ├── decay_scheduler.hpp
│   │   │   └── activity_monitor.hpp
│   │   │
│   │   └── reputation/            # Trust & reputation system
│   │       ├── reputation.hpp
│   │       ├── reputation.cpp
│   │       ├── trust_graph.hpp
│   │       ├── attestation.hpp
│   │       └── vouching.hpp
│   │
│   ├── crypto/                    # Cryptography primitives
│   │   ├── CMakeLists.txt
│   │   ├── ed25519.hpp            # Ed25519 signatures
│   │   ├── ed25519.cpp
│   │   ├── x25519.hpp             # X25519 key exchange
│   │   ├── x25519.cpp
│   │   ├── chacha20poly1305.hpp   # Symmetric encryption
│   │   ├── chacha20poly1305.cpp
│   │   ├── blake3.hpp             # BLAKE3 hashing
│   │   ├── blake3.cpp
│   │   ├── argon2.hpp             # Argon2 for PoW
│   │   ├── argon2.cpp
│   │   ├── random.hpp             # CSPRNG
│   │   ├── random.cpp
│   │   └── crypto_utils.hpp
│   │
│   ├── network/                   # P2P networking
│   │   ├── CMakeLists.txt
│   │   ├── transport/             # Encrypted transport layer
│   │   │   ├── transport.hpp
│   │   │   ├── transport.cpp
│   │   │   ├── session.hpp
│   │   │   ├── session.cpp
│   │   │   ├── handshake.hpp
│   │   │   └── connection.hpp
│   │   │
│   │   ├── routing/               # Routing protocol
│   │   │   ├── router.hpp
│   │   │   ├── router.cpp
│   │   │   ├── routing_table.hpp
│   │   │   ├── content_router.hpp
│   │   │   ├── onion_routing.hpp
│   │   │   └── multi_hop.hpp
│   │   │
│   │   ├── gossip/                # Gossip protocol
│   │   │   ├── gossip.hpp
│   │   │   ├── gossip.cpp
│   │   │   ├── message.hpp
│   │   │   ├── propagation.hpp
│   │   │   └── deduplication.hpp
│   │   │
│   │   └── discovery/             # Discovery layer
│   │       ├── discovery.hpp
│   │       ├── discovery.cpp
│   │       ├── bootstrap.hpp
│   │       ├── peer_discovery.hpp
│   │       └── dht.hpp (optional)
│   │
│   ├── storage/                   # Content-addressed storage
│   │   ├── CMakeLists.txt
│   │   ├── storage.hpp
│   │   ├── storage.cpp
│   │   ├── content_store.hpp
│   │   ├── metadata_store.hpp
│   │   ├── chunking.hpp
│   │   └── deduplication.hpp
│   │
│   ├── gateway/                   # Web gateway (HTTPS)
│   │   ├── CMakeLists.txt
│   │   ├── gateway.hpp
│   │   ├── gateway.cpp
│   │   ├── http_server.hpp
│   │   ├── websocket_server.hpp
│   │   ├── api_handler.hpp
│   │   └── streaming.hpp
│   │
│   ├── security/                  # Attack prevention & anonymity
│   │   ├── CMakeLists.txt
│   │   ├── capability_token.hpp
│   │   ├── capability_token.cpp
│   │   ├── rate_limiter.hpp
│   │   ├── anti_sybil.hpp
│   │   ├── fork_detection.hpp
│   │   └── anonymity.hpp
│   │
│   └── utils/                     # Utilities
│       ├── CMakeLists.txt
│       ├── logger.hpp             # Logging
│       ├── logger.cpp
│       ├── config.hpp             # Configuration
│       ├── config.cpp
│       ├── time.hpp               # Time utilities
│       ├── serialization.hpp      # MessagePack/Protobuf
│       ├── hex.hpp                # Hex encoding
│       ├── base64.hpp             # Base64 encoding
│       └── error.hpp              # Error handling
│
├── tests/                         # Unit and integration tests
│   ├── CMakeLists.txt
│   ├── test_main.cpp              # Test runner
│   ├── crypto/                    # Crypto tests
│   │   ├── test_ed25519.cpp
│   │   ├── test_x25519.cpp
│   │   └── test_blake3.cpp
│   ├── core/                      # Core tests
│   │   ├── test_node.cpp
│   │   ├── test_thing.cpp
│   │   └── test_network.cpp
│   ├── network/                   # Network tests
│   │   ├── test_transport.cpp
│   │   ├── test_routing.cpp
│   │   └── test_gossip.cpp
│   └── integration/               # Integration tests
│       ├── test_end_to_end.cpp
│       └── test_multi_node.cpp
│
├── benchmarks/                    # Performance benchmarks
│   ├── CMakeLists.txt
│   ├── bench_main.cpp
│   ├── bench_crypto.cpp
│   ├── bench_pow.cpp
│   └── bench_routing.cpp
│
├── examples/                      # Example applications
│   ├── CMakeLists.txt
│   ├── simple_node.cpp            # Simple node example
│   ├── host_thing.cpp             # Thing hosting example
│   ├── join_network.cpp           # Network joining example
│   └── gateway_example.cpp        # Gateway example
│
├── web/                           # Web interface (HTML/CSS/JS)
│   ├── index.html                 # Main page
│   ├── css/
│   │   └── style.css
│   ├── js/
│   │   ├── cashew_client.js       # Gateway API client
│   │   ├── game_renderer.js
│   │   └── mona.js                # Mascot animations
│   ├── assets/
│   │   └── mona/                  # Mona mascot images
│   └── games/                     # Sample games
│       ├── tetris/
│       └── snake/
│
├── docs/                          # Documentation
│   ├── architecture.md            # System architecture
│   ├── protocol.md                # Protocol details
│   ├── api_reference.md           # API documentation
│   ├── setup_guide.md             # Setup instructions
│   ├── developer_guide.md         # Developer guide
│   └── diagrams/                  # Architecture diagrams
│       ├── node_architecture.png
│       ├── network_topology.png
│       └── protocol_stack.png
│
├── tools/                         # Build and development tools
│   ├── generate_keys.cpp          # Key generation utility
│   ├── thing_packager.cpp         # Thing packaging tool
│   ├── network_monitor.cpp        # Network monitoring tool
│   └── benchmark_node.cpp         # Node benchmarking tool
│
└── third_party/                   # External dependencies
    ├── libsodium/                 # (if built from source)
    ├── blake3/                    # BLAKE3 library
    └── argon2/                    # Argon2 library
```

## Key Files Overview

### Build System
- **CMakeLists.txt**: Root build configuration with cross-platform support
- **cmake/**: CMake modules for dependency finding and configuration
- **src/CMakeLists.txt**: Source-level build configuration

### Core Headers
- **include/cashew/common.hpp**: Common types, constants, and base definitions
- **include/cashew/version.hpp**: Version information
- **include/cashew/cashew.hpp**: Main umbrella header

### Main Entry Point
- **src/main.cpp**: Node executable entry point

### Core Modules (src/core/)
1. **node/**: Node system implementation
2. **thing/**: Content (Thing) management
3. **network/**: Network cluster management
4. **identity/**: Identity systems (machine & human)
5. **keys/**: Participation key system
6. **pow/**: Adaptive Proof-of-Work
7. **postake/**: Proof-of-Stake contribution tracking
8. **ledger/**: Distributed state management
9. **decay/**: Decay and cleanup system
10. **reputation/**: Trust and reputation

### Cryptography (src/crypto/)
- Ed25519 signatures
- X25519 key exchange
- ChaCha20-Poly1305 encryption
- BLAKE3 hashing
- Argon2 for PoW
- CSPRNG

### Networking (src/network/)
1. **transport/**: Encrypted session layer
2. **routing/**: Content-addressed routing
3. **gossip/**: Gossip protocol
4. **discovery/**: Peer discovery and DHT

### Storage (src/storage/)
- Content-addressed storage
- Metadata management
- Chunking and deduplication

### Gateway (src/gateway/)
- HTTP/HTTPS server
- WebSocket support
- API handlers
- Content streaming

### Security (src/security/)
- Capability tokens
- Rate limiting
- Anti-Sybil measures
- Fork detection
- Anonymity features

### Utilities (src/utils/)
- Logging
- Configuration
- Serialization
- Time utilities
- Error handling

### Testing
- Unit tests for all modules
- Integration tests
- End-to-end tests

### Web Interface
- HTML/CSS/JS frontend
- Gateway API client
- Game renderers
- Mona mascot integration

### Documentation
- Architecture documentation
- Protocol specification
- API reference
- User guides
- Developer guides

## Build Instructions

### Prerequisites
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install cmake ninja-build libsodium-dev

# Or use vcpkg for cross-platform dependency management
```

### Building
```bash
# Create build directory
mkdir build && cd build

# Configure with CMake (using Ninja)
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..

# Build
ninja

# Run tests
ctest

# Install
sudo ninja install
```

### Cross-Platform
```bash
# Windows (MSVC)
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release

# Linux (GCC/Clang)
cmake -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
ninja
```

## Module Dependencies

```
cashew_core
├── utils (logging, config, time)
├── crypto (all cryptographic primitives)
├── storage (content & metadata)
├── core
│   ├── identity
│   ├── keys (depends on crypto, identity)
│   ├── pow (depends on crypto, keys)
│   ├── postake (depends on keys)
│   ├── ledger (depends on crypto, identity)
│   ├── node (depends on identity, keys, pow, postake)
│   ├── thing (depends on crypto, storage, node)
│   ├── network (depends on thing, node, keys)
│   ├── decay (depends on keys, thing)
│   └── reputation (depends on identity, ledger)
├── network
│   ├── transport (depends on crypto)
│   ├── routing (depends on transport, thing)
│   ├── gossip (depends on transport)
│   └── discovery (depends on transport, gossip)
├── security (depends on crypto, keys, reputation)
└── gateway (depends on network, thing, security)
```

## Next Steps

1. Implement basic crypto wrappers (Ed25519, X25519, ChaCha20, BLAKE3)
2. Build Node and Identity systems
3. Implement PoW with adaptive difficulty
4. Create Thing storage and management
5. Develop transport and session layer
6. Build gossip protocol
7. Implement routing
8. Add Network cluster management
9. Create gateway for web access
10. Integrate all components and test end-to-end
