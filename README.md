# Cashew Network

yeah it's another P2P thing. but this one actually compiles.

**Status:** it works. probably. builds clean at least.  
**Lines of code:** too many (38k+)  
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

### supported platforms

**Operating Systems:**
- Windows 10/11 (MSYS2/MinGW)
- Linux (Ubuntu, Debian, Arch, Fedora, etc.)
- macOS (Intel & Apple Silicon)

**CPU Architectures:**
- x86_64 (Intel/AMD 64-bit)
- x86 (Intel/AMD 32-bit)
- ARM64/AArch64 (Raspberry Pi 3/4/5 64-bit, Apple M1/M2, AWS Graviton)
- ARMv7 (Raspberry Pi 2/3/4 32-bit)

Cashew automatically detects your architecture and enables appropriate optimizations (SSE/AVX on x86, NEON on ARM). See [docs/ARM_SUPPORT.md](docs/ARM_SUPPORT.md) for Raspberry Pi details.

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

git clone https://github.com/Plutomaster28/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# binary at build/src/cashew.exe
```

### linux

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build \
                 libsodium-dev libspdlog-dev nlohmann-json3-dev

git clone https://github.com/Plutomaster28/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# binary at build/src/cashew
```

### raspberry pi

```bash
# Quick setup (installs dependencies + builds)
git clone https://github.com/Plutomaster28/cashew.git
cd cashew
chmod +x setup-raspberry-pi.sh
./setup-raspberry-pi.sh

# binary at build/src/cashew
```

**Raspberry Pi notes:**
- Fully supported on Pi 2/3/4/5 (both 32-bit and 64-bit OS)
- ARM NEON SIMD acceleration enabled automatically
- BLAKE3 uses ARM crypto extensions where available
- See [docs/ARM_SUPPORT.md](docs/ARM_SUPPORT.md) for performance tips

### macOS

```bash
brew install cmake ninja libsodium spdlog nlohmann-json

git clone https://github.com/Plutomaster28/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# binary at build/src/cashew
```

---

## how to actually use this thing

### step 1: start your node

```bash
# Windows
.\build\src\cashew.exe

# Linux/macOS  
./build/src/cashew
```

or explicitly:

```bash
./build/src/cashew node
```

you'll see:
```
Cashew node is running!
  Gateway:    http://localhost:8080
  WebSocket:  ws://localhost:8080/ws
  Node ID:    2ada83c1819a5372...
```

the gateway serves:
- `/` - basic status page (hardcoded HTML)
- `/api/status` - JSON with node info
- `/api/networks` - JSON list of your networks
- `/api/thing/{hash}` - retrieve content by BLAKE3 hash
- `/static/*` - static files from configured `web_root`

if `web_root/index.html` exists, `/` serves that file so normal browsers can open your hosted site directly.
root-relative assets like `/style.css` and `/app.js` are also served from `web_root` for regular static-site behavior.

### step 2: understand what you just started

cashew node does:
- stores content in `./data/storage/content/` (files by hash)
- tracks metadata in `./data/storage/metadata/` (MIME types)
- logs events to `./data/ledger/events.db` (sqlite)
- exposes HTTP gateway on port 8080

**important:** the gateway just gives you API access. to actually HOST a website and let browsers view it, you need a frontend gateway (see step 3).

### step 3: host actual content

you have two options:

**option A: use the perl CGI example (recommended for testing)**

the perl gateway lets browsers actually VIEW your content:

```bash
# install perl (MSYS2)
pacman -S mingw-w64-ucrt-x86_64-perl \
          mingw-w64-ucrt-x86_64-perl-cgi \
          mingw-w64-ucrt-x86_64-perl-json

# go to example
cd examples/perl-cgi

# start perl gateway on port 8081
perl server.pl
```

now you have:
- cashew node: localhost:8080 (stores content)
- perl gateway: localhost:8081 (serves content to browsers)

the perl gateway fetches from cashew's `/api/thing/{hash}` and serves it as regular HTML/CSS/images.

**option B: write your own frontend**

cashew provides the API. you build whatever frontend you want:
- react app
- static site generator
- mobile app
- whatever

just hit `/api/status`, `/api/networks`, `/api/thing/{hash}`.

### step 4: upload content with the CLI

no more manual file copying. use the built-in CLI:

```bash
# add one file to local content storage
./build/src/cashew content add ./my-site/index.html

# optional mime override
./build/src/cashew content add ./my-site/index.html --mime "text/html; charset=utf-8"

# list all locally stored content hashes
./build/src/cashew content list
```

the add command prints the content hash and API URL.

### step 5: share content links

```bash
# print shareable URLs for a known hash
./build/src/cashew share <64-char-hash>

# if your gateway is running on another host/port
./build/src/cashew share <64-char-hash> --gateway http://192.168.1.20:8080

# default internet URL can come from config key: public_gateway
# then plain `cashew share <hash>` prints public links
```

share the generated `/api/thing/<hash>` URL with other people on your network.

anyone with browser access to your exposed gateway URL can fetch shared content via `/api/thing/<hash>`.
they do not need a Cashew node installed for read-only access.

### step 6: host an actual site (festival-safe version)

1. start Cashew node (`cashew node`) on host machine
2. upload site assets with `cashew content add ...`
3. keep a list of generated hashes (index.html, css, js, images)
4. run the Perl frontend (`examples/perl-cgi/server.pl`) for browser-friendly pages
5. map asset references in your HTML to the shared `/api/thing/<hash>` endpoints
6. test from a second device on LAN before demo day

for now, the cleanest public demo flow is single-node or trusted-LAN node sharing.

for internet/public deployment patterns (self-hosted TLS, onion service, relay model), see `docs/HOSTING_AND_SHARING.md`.

for config-based public links, set this in `cashew.conf`:

```json
{
  "gateway": {
    "http_port": 8080,
    "public_gateway": "https://your-domain.example"
  }
}
```

then `cashew share <hash>` prints internet-facing links automatically.

**one-command option (windows/powershell):**

```powershell
./scripts/festival-bootstrap.ps1 -AssetDir test-site -Gateway http://localhost:8080
```

this will build, start the node, ingest all files under `test-site/`, and write `festival-links.csv` with shareable URLs.

### production/festival checklist

before demo day:

1. build in release mode (`cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build`)
2. run tests (`ctest --test-dir build --output-on-failure`)
3. start node (`./build/src/cashew node`)
4. upload all site assets with `cashew content add ...`
5. save the generated hashes in a small cheat sheet
6. verify each shared URL from a second machine on the same network
7. keep a backup copy of `data/` and `cashew_identity.dat` before travel

during demo:

1. start node first, wait for `Cashew node is running!`
2. share URLs generated by `cashew share <hash> --gateway http://<demo-host>:8080`
3. if browser rendering looks wrong, re-add asset with explicit MIME using `--mime`
4. if you must restart, do a clean Ctrl+C shutdown so network/storage state is saved

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
1. gateway gets HTTP request: GET /api/thing/abc123
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
- [HOSTING_AND_SHARING.md](docs/HOSTING_AND_SHARING.md) - how to host/share on LAN and public web
- [deploy/README.md](deploy/README.md) - edge proxy configs for internet serving
- [PROTOCOL_SPEC.md](PROTOCOL_SPEC.md) - technical protocol details
- [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) - roadmap
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) - codebase layout

the user manual is actually good, has diagrams and everything.

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
├── tests/             # unit tests
├── examples/          # perl-cgi gateway etc
└── docs/              # documentation
```

clean build with -Werror, no warnings. 15k+ lines across 37 files.

---

## known issues

**sharing maturity:**
- invitation signing and verification are implemented in the network layer
- network state persistence is implemented with on-disk binary snapshots

**general:**
- web UI is basic
- performance tuning pending for larger node counts
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

it's a nut. seemed cute.

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

---

## contact

- issues: https://github.com/Plutomaster28/cashew/issues
- email: probably should set one up
- discord: maybe later

---

**tagline:** cashew - it compiles, ships, and probably works

*freedom over profit. privacy over surveillance.*

---

last updated: march 26, 2026  
version: 0.1.0  
status: somehow functional

