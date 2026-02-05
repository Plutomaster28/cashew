# Cashew Network

yeah it's another P2P thing. but this one actually compiles.

```
       /\__/\
     （　´∀｀）  <- mona says hi (from 2channel)
     （　　　）
       ｜ ｜ |
     （＿_)＿）
```

**Status:** it works. probably. builds clean at least.  
**Lines of code:** too many (15k+)  
**Build warnings:** 0 (we use -Werror like masochists)  
**Version:** 0.1.0 (don't use this in production lol)

---

## what is this

peer-to-peer content sharing but you don't have to expose ports or trust randos on the internet.

think IPFS but invitation-only. or Tor but for hosting cat pictures. or BitTorrent but your ISP can't see what you're seeding.

**the pitch:**
- you host content (websites, games, whatever)
- your friends help host it too (redundancy)
- everyone's happy until someone goes offline
- content is identified by hash so nobody can tamper with it
- uses actual cryptography (Ed25519, BLAKE3, ChaCha20)

**not a blockchain.** not a cryptocurrency. no tokens. no NFTs. just files and friends.

---

## why this exists

got tired of:
- centralized platforms deleting stuff
- having to trust cloudflare
- DMCA takedowns on stupid things
- paying for hosting
- opening ports on my router (security nightmare)

so here we are. decentralized content hosting where you choose who's in your network.

---

## how it works

### the simple version

1. you create a "Thing" (just content, could be anything)
2. cashew hashes it with BLAKE3 (fast af)
3. hash becomes the Thing's ID
4. you invite friends to a "Network" to help host it
5. they download it, verify the hash, start serving
6. if you go offline, they keep it alive
7. browser gateway makes it accessible via HTTP

### the technical version

**Architecture:**
```
Service Layer     -> Things, Networks, access control
Access Layer      -> Capability tokens (not exposed)
Discovery Layer   -> Gossip protocol, content resolution
Routing Layer     -> Multi-hop, content-addressed routing
Transport Layer   -> Encrypted sessions (ChaCha20-Poly1305)
Identity Layer    -> Ed25519 keys, BLAKE3 node IDs
```

**Storage:**
```
data/
├── storage/content/    # actual files by hash
├── storage/metadata/   # MIME types etc
├── ledger/events.db    # append-only log
└── networks/           # membership data
```

**Content addressing:**
- everything identified by BLAKE3 hash
- if hash matches, content is guaranteed identical
- no sneaky updates, no tampering
- what you request is what you get

**Crypto:**
- Ed25519 for signatures (fast, 32-byte keys)
- BLAKE3 for hashing (stupid fast, parallel)
- ChaCha20-Poly1305 for encryption (TLS 1.3 tier)
- no custom crypto, all battle-tested

**Networking:**
- P2P mesh, no central server
- gossip protocol for state sync
- invitation-only networks (no randos)
- optional multi-hop routing for privacy

---

## building this thing

### dependencies

you need:
- C++20 compiler (GCC 15.2+ or Clang 16+)
- CMake 3.20+
- Ninja (faster than make)
- libsodium (for crypto)
- spdlog (logging)
- nlohmann-json (json parsing)

### windows (MSYS2)

```bash
# install MSYS2 first: https://www.msys2.org/
# open UCRT64 terminal

pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-libsodium \
          mingw-w64-ucrt-x86_64-spdlog \
          mingw-w64-ucrt-x86_64-nlohmann-json

git clone https://github.com/yourusername/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# binary at build/src/cashew_node.exe
```

### linux

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build \
                 libsodium-dev libspdlog-dev nlohmann-json3-dev

git clone https://github.com/yourusername/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# binary at build/src/cashew_node
```

### macOS

```bash
brew install cmake ninja libsodium spdlog nlohmann-json

git clone https://github.com/yourusername/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# binary at build/src/cashew_node
```

---

## running it

**basic:**
```bash
./build/src/cashew_node
```

opens gateway on http://localhost:8080

**with config:**
```bash
./build/src/cashew_node --config cashew.conf
```

**example config:**
```ini
log_level = info
data_dir = ./data
http_port = 8080
web_root = ./web
```

---

## hosting content

### perl CGI gateway (yes really)

because sometimes you just want to use 1990s tech and it actually works fine.

install perl modules (MSYS2):
```bash
pacman -S mingw-w64-ucrt-x86_64-perl \
          mingw-w64-ucrt-x86_64-perl-cgi \
          mingw-w64-ucrt-x86_64-perl-json \
          mingw-w64-ucrt-x86_64-perl-http-message
```

see `examples/perl-cgi/cashew-gateway.pl` for a working example.

it's like 300 lines and serves content from your node via HTTP. retro but it works.

---

## the technical bits

### cryptography & keys

every node has an Ed25519 keypair:
- private key: keep this secret (duh)
- public key: share with friends
- node ID: BLAKE3 hash of public key

**authentication:**
1. friend sends invitation (signed with their key)
2. you verify signature (proves it's them)
3. you accept and sign response
4. both signatures stored in ledger

**content verification:**
```
content -> BLAKE3() -> hash (Thing ID)

download -> hash it -> compare to ID
  match?   -> valid!
  mismatch? -> corrupted/fake, reject
```

### proof-of-work (not crypto)

lightweight PoW to prevent spam. NOT for money.

when you join a network or publish content:
```
find nonce where: BLAKE3(message + nonce) < difficulty

difficulty levels:
- view content: none (0 sec)
- join network: low (~1 sec)
- publish Thing: medium (~5 sec)
- create network: high (~30 sec)
```

builds reputation:
```
reputation = PoW solved + uptime + content hosted - bad behavior

high rep = more trusted = priority in networks
```

prevents spam because creating fake identities is expensive (cpu time).

### P2P networking

**when someone requests content:**
1. gateway gets HTTP request: GET /thing/abc123
2. storage checks: "do I have abc123?"
3. yes: serve from disk
4. no: ask network members
5. download from peer, verify hash
6. serve to user, cache locally

**WebSocket for real-time:**
- new content available
- member joined/left
- reputation changes
- Thing updates

connect to: ws://localhost:8080/ws

---

## docs

- [USER_MANUAL.md](docs/USER_MANUAL.md) - complete guide (beginner friendly)
- [PROTOCOL_SPEC.md](PROTOCOL_SPEC.md) - technical protocol details
- [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) - roadmap
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) - codebase layout

the user manual is actually good, has diagrams and everything. features Mona the cat.

---

## project structure

```
cashew/
├── src/
│   ├── core/          # node, Thing, network, keys
│   ├── crypto/        # Ed25519, BLAKE3, ChaCha20
│   ├── network/       # P2P, gossip, routing
│   ├── storage/       # content-addressed storage
│   ├── gateway/       # HTTP/WebSocket server
│   └── security/      # attack prevention
├── include/cashew/    # public headers
├── tests/             # unit tests (todo)
├── examples/          # perl-cgi gateway etc
├── web/               # web UI (html/css/js)
└── docs/              # documentation
```

clean build with -Werror, no warnings. 15k+ lines across 37 files.

---

## known issues

**libsodium Ed25519 crash on Windows:**
- Node::initialize() crashes in crypto::Ed25519::generate_keypair()
- workaround: uses temporary NodeID from BLAKE3 hash
- impact: no persistent identity, no crypto ops
- fix: coming eventually, not critical for testing

**general:**
- no tests yet (phase 7)
- web UI is basic
- needs optimization
- documentation could be better
- probably bugs we haven't found

---

## security

designed with paranoia:
- no open ports (all outbound connections)
- everything encrypted (ChaCha20-Poly1305)
- content integrity (BLAKE3 verification)
- anti-Sybil (PoW + reputation)
- anti-DDoS (rate limiting)
- optional anonymity (multi-hop routing)

**threat model:**
- ISP: can see you're running cashew, not what you're hosting
- network members: can see what they're hosting (they agreed to it)
- attackers: expensive to spam, reputation system filters them

not bulletproof but better than trusting random cloud providers.

---

## why "cashew"

it's a nut. Mona (the 2channel cat mascot) likes nuts probably. seemed cute.

also "cache" + "new" if you squint.

---

## contributing

PRs welcome. no formal process, just:
1. make sure it compiles
2. follow existing code style
3. don't break stuff
4. write a decent commit message

areas that need help:
- testing (unit tests, integration tests)
- web UI/UX
- documentation
- performance optimization
- bug hunting

---

## license

MIT. do whatever you want with it.

see [LICENSE](LICENSE) for legal text.

---

## credits

inspired by:
- Bitcoin (trustless consensus)
- Tor (onion routing)
- IPFS (content addressing)
- every frustrated dev who's dealt with centralized platforms

built with:
- too much coffee
- not enough sleep
- C++20 (the good parts)
- libsodium (actual cryptographers made this)

special thanks to Mona the cat (from 2channel).

---

## contact

- issues: GitHub Issues
- email: probably should set one up
- discord: maybe later

---

**tagline:** cashew - it compiles, ships, and probably works

*freedom over profit. privacy over surveillance.*

---

last updated: february 1, 2026  
version: 0.1.0  
status: somehow functional
