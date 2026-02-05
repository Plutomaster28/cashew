# Cashew Network - User Manual

```
     _______________________________________________
    /                                               \
   |   ___      _     _                             |
   |  / __|__ _| |_  | |_  _____ __ __              |
   |  | (__/ _` | _| | ' \/ -_) V  V /              |
   |   \___\__,_|\__| |_||_\___|\_/\_/               |
   |                                                 |
   |  Freedom over profit. Privacy over surveillance.|
   |                                                 |
   |        /\__/\                                   |
   |      （　´∀｀）＜ "Share freely!"               |
   |      （　　　）                                 |
   |        ｜ ｜ |                                  |
   |      （＿_)＿）                                |
    \_______________________________________________/
```

**Version:** 0.1.0 (February 2026)  
**For:** Windows, Linux, macOS  
**Difficulty:**  Easy Peasy!

---

##  Table of Contents

1. [Welcome to Cashew!](#welcome)
2. [What is Cashew?](#what-is-cashew)
3. [Installation](#installation)
4. [Quick Start Guide](#quick-start)
5. [Creating Your First Website](#creating-website)
6. [Hosting with Perl + CGI](#perl-hosting)
7. [Understanding Things](#things)
8. [Managing Networks](#networks)
9. [How Hosting Works](#hosting)
10. [Cryptography & Key System](#crypto)
11. [Proof-of-Work & Reputation](#pow)
12. [Troubleshooting](#troubleshooting)
13. [FAQ](#faq)

---

<a name="welcome"></a>
##  Welcome to Cashew!

```
       /\__/\
     （　´∀｀）  <- This is Mona, your friendly Cashew guide!
     （　　　）      (from 2channel, the OG Japanese forum)
       ｜ ｜ |
     （＿_)＿）
```

**Hey there, new friend!** 

Thank you for joining the Cashew network! You're about to embark on a journey where **you** own your content, **you** control who sees it, and **nobody** can take it away from you.

No corporations. No ads. No tracking. Just pure, wholesome content sharing!

### What Makes Cashew Special?

- **You own your data** - It's stored on YOUR computer
- **Privacy first** - No one can spy on what you host
- **Invite-only sharing** - You choose who joins your network
- **Completely free** - No subscriptions, no hidden fees
- **Host anything** - Websites, blogs, photos, whatever!

---

<a name="what-is-cashew"></a>
##  What is Cashew?

Imagine if you and your friends could create a mini-Internet that only you control. That's Cashew!

### The Simple Explanation

**Cashew lets you:**
1. Create a "Thing" (like a website or photo album)
2. Invite friends to help host it
3. Share it with people you trust
4. Everyone helps keep it online

**Think of it like:**
- A library where every book has multiple copies
- A neighborhood where everyone backs up each other's photos
- A game server that your friends help run

### The Technical Explanation

Cashew is a **peer-to-peer content distribution network** with:
- Content-addressed storage (everything identified by its hash)
- Invitation-only networks (no random people)
- Automatic redundancy (multiple copies for safety)
- Append-only ledger (permanent record of everything)
- Built-in reputation system (trust through contribution)

---

<a name="installation"></a>
##  Installation

### Windows (MSYS2/MinGW)

**Step 1: Install Prerequisites**
```powershell
# Install MSYS2 from https://www.msys2.org/
# Then open MSYS2 UCRT64 terminal and run:

pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-libsodium \
          mingw-w64-ucrt-x86_64-spdlog \
          mingw-w64-ucrt-x86_64-nlohmann-json
```

**Step 2: Build Cashew**
```powershell
# Clone the repository
git clone https://github.com/yourusername/cashew.git
cd cashew

# Build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# You now have cashew_node.exe in build/src/
```

**Step 3: Install Perl CGI Dependencies (MSYS2)**
```bash
# Install Perl and required modules via MSYS2 package manager
pacman -S mingw-w64-ucrt-x86_64-perl \
          mingw-w64-ucrt-x86_64-perl-cgi \
          mingw-w64-ucrt-x86_64-perl-json \
          mingw-w64-ucrt-x86_64-perl-http-message

# If any module is missing from repos, use cpanm
pacman -S mingw-w64-ucrt-x86_64-perl-app-cpanminus
cpanm HTTP::Tiny
```

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake ninja-build \
                 libsodium-dev libspdlog-dev nlohmann-json3-dev

# Clone and build
git clone https://github.com/yourusername/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# Binary is at build/src/cashew_node
```

### macOS

```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake ninja libsodium spdlog nlohmann-json

# Clone and build
git clone https://github.com/yourusername/cashew.git
cd cashew
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# Binary is at build/src/cashew_node
```

---

<a name="quick-start"></a>
##  Quick Start Guide

### Starting Your Node (5 Minutes!)

```
    Mona says: "Let's get you online in 5 minutes!"
    
       /\__/\
     （　´∀｀）
     （　　　）
       ｜ ｜ |
     （＿_)＿）
```

**Step 1: Create a config file (optional)**

Create `cashew.conf` in your Cashew directory:

```ini
# Cashew Node Configuration

# Logging
log_level = info
log_to_file = false

# Data storage
data_dir = ./data

# Gateway server
http_port = 8080
web_root = ./web

# Identity (leave empty for auto-generation)
identity_file = cashew_identity.dat
identity_password =
```

**Step 2: Start your node!**

```bash
# Windows (PowerShell)
.\build\src\cashew_node.exe

# Linux/macOS
./build/src/cashew_node
```

**Step 3: Open your browser**

Navigate to: **http://localhost:8080**

**You're running a Cashew node!**

### What You'll See

```
Cashew node is running!

  Gateway:    http://localhost:8080
  WebSocket:  ws://localhost:8080/ws
  Web UI:     http://localhost:8080/

  Node ID:    2ada83c1819a5372dae1238fc1ded123c8104fdaa15862aaee69428a1820fcda
  Storage:    0 items
  Networks:   0
  Ledger:     0 events

Press Ctrl+C to shutdown
```

---

<a name="creating-website"></a>
## Creating Your First Website

Let's create a simple website and share it on Cashew!

### The Old-School Way: Static HTML

**Step 1: Create your website**

Create a folder `my-first-site/` with:

```html
<!-- index.html -->
<!DOCTYPE html>
<html>
<head>
    <title>My Cashew Site!</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <h1> Welcome to My Cashew Site!</h1>
    <p>This is my first website on the Cashew network!</p>
    <p>No corporations. No tracking. Just me and my friends.</p>
</body>
</html>
```

```css
/* style.css */
body {
    font-family: Arial, sans-serif;
    max-width: 800px;
    margin: 50px auto;
    padding: 20px;
    background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
}

h1 {
    color: #8B4513;
    text-align: center;
}
```

**Step 2: Store it in Cashew**

```bash
# Coming soon: cashew-cli tool
# For now, we'll use Perl + CGI!
```

---

<a name="perl-hosting"></a>
## Hosting with Perl + CGI (The Fun Way!)

```
    Mona says: "Perl is like the grandfather of the web.
                Old, reliable, and surprisingly powerful!"
    
       /\__/\
     （　´∀｀）
     （　　　）
       ｜ ｜ |
     （＿_)＿）
```

### Why Perl + CGI?

- **Lightweight** - Perfect for Raspberry Pi or old computers
- **No build step** - Just write and run
- **Portable** - Works everywhere Perl works
- **Simple** - Great for learning
- **Nostalgia** - Like the 90s web, but better!

### Setting Up a Perl CGI Gateway

**Step 1: Install Perl** (usually pre-installed on Linux/macOS)

```bash
# Check if you have Perl
perl --version

# If not, install it:
# Ubuntu/Debian
sudo apt install perl

# macOS (via Homebrew)
brew install perl
```

**Step 2: Create a simple CGI script**

Create `cashew-gateway.pl`:

```perl
#!/usr/bin/env perl
use strict;
use warnings;
use CGI;
use JSON;
use LWP::UserAgent;

# Configuration
my $CASHEW_API = "http://localhost:8080/api";

# Create CGI object
my $cgi = CGI->new;

# Get the requested path
my $path = $cgi->path_info() || '/';

# Route the request
if ($path =~ m{^/thing/([a-f0-9]{64})$}) {
    serve_thing($1);
} elsif ($path eq '/networks') {
    list_networks();
} elsif ($path =~ m{^/network/(.+)$}) {
    show_network($1);
} else {
    show_home();
}

sub serve_thing {
    my ($hash) = @_;
    my $ua = LWP::UserAgent->new;
    my $response = $ua->get("$CASHEW_API/thing/$hash");
    
    if ($response->is_success) {
        print $cgi->header(
            -type => $response->header('Content-Type'),
            -charset => 'utf-8'
        );
        print $response->decoded_content;
    } else {
        print $cgi->header(-status => '404 Not Found');
        print "<h1>Thing not found</h1>";
    }
}

sub list_networks {
    my $ua = LWP::UserAgent->new;
    my $response = $ua->get("$CASHEW_API/networks");
    
    if ($response->is_success) {
        my $data = decode_json($response->decoded_content);
        
        print $cgi->header(-type => 'text/html', -charset => 'utf-8');
        print <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <title>Cashew Networks</title>
    <style>
        body { font-family: Arial; max-width: 800px; margin: 50px auto; }
        .network { border: 1px solid #ccc; padding: 15px; margin: 10px 0; }
        .healthy { background: #d4edda; }
        .degraded { background: #fff3cd; }
    </style>
</head>
<body>
    <h1> Active Networks</h1>
HTML
        
        foreach my $net (@{$data->{networks}}) {
            my $health_class = $net->{health} eq 'healthy' ? 'healthy' : 'degraded';
            print "<div class='network $health_class'>";
            print "<h2>$net->{name}</h2>";
            print "<p><strong>Members:</strong> $net->{member_count}</p>";
            print "<p><strong>Status:</strong> $net->{health}</p>";
            print "</div>";
        }
        
        print "</body></html>";
    }
}

sub show_home {
    print $cgi->header(-type => 'text/html', -charset => 'utf-8');
    print <<'HTML';
<!DOCTYPE html>
<html>
<head>
    <title>Cashew Gateway</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
        }
        h1 { color: #8B4513; text-align: center; }
        .ascii { font-family: monospace; text-align: center; }
    </style>
</head>
<body>
    <div class="ascii">
<pre>
       /\__/\
     （　´∀｀）  <- Mona welcomes you!
     （　　　）
       ｜ ｜ |
     （＿_)＿）
</pre>
    </div>
    <h1> Cashew Gateway</h1>
    <p>Welcome to your personal gateway to the Cashew network!</p>
    
    <h2>Quick Links</h2>
    <ul>
        <li><a href="/networks">View Networks</a></li>
        <li><a href="http://localhost:8080">Full Web UI</a></li>
    </ul>
    
    <h2>About This Gateway</h2>
    <p>This is a simple Perl CGI gateway running on your local machine.
       It connects to your Cashew node and lets you browse content!</p>
</body>
</html>
HTML
}
```

**Step 3: Make it executable and run**

```bash
chmod +x cashew-gateway.pl

# Run with Perl's built-in HTTP server
perl -MCGI::HTTPServer -e 'my $server=CGI::HTTPServer->new(port=>8081); $server->run();'

# Or use Apache/nginx with CGI support
```

**Step 4: Access your gateway**

Open: **http://localhost:8081/cashew-gateway.pl**

---

<a name="things"></a>
##  Understanding Things

### What is a Thing?

A **Thing** is any piece of content in Cashew:
- Websites
- Documents
- Images
- Music
- Videos
- Literally anything!

### How Things Work

```
1. You create content
   └─→ 2. Cashew hashes it (SHA-256)
         └─→ 3. Hash becomes the Thing's ID
               └─→ 4. Content stored at ./data/storage/content/<hash>
                     └─→ 5. Friends can request it by hash
```

### Content Addressing is Magic! 

```
    Traditional Web:              Cashew Network:
    
    example.com/page.html   →     cashew://abc123.../
         ↓                              ↓
    Server controls it         Content IS the address!
    Can change anytime         Hash never changes!
    Can disappear              Lives forever!
```

**Key insight:** If the hash matches, the content is EXACTLY what you asked for. No tampering possible!

---

<a name="networks"></a>
##  Managing Networks

### What is a Network?

A **Network** is a group of friends who agreed to host ONE specific Thing together.

```
    Network "My Cool Blog"
         ↓
    ┌─────────────────────┐
    │  Thing: abc123...   │
    ├─────────────────────┤
    │  Members:           │
    │  • You (Founder)    │
    │  • Alice (Full)     │
    │  • Bob (Full)       │
    │  • Carol (Observer) │
    └─────────────────────┘
```

### Creating a Network

```bash
# Via API (example)
curl -X POST http://localhost:8080/api/network/create \
  -H "Content-Type: application/json" \
  -d '{
    "thing_hash": "abc123...",
    "name": "My Cool Blog",
    "quorum": {
      "min_replicas": 3,
      "target_replicas": 5,
      "max_replicas": 10
    }
  }'
```

### Inviting Friends

Networks are **invitation-only**! You can't just join - someone must invite you.

**Why?**
-  **Privacy** - Only people you trust
-  **Purpose** - Everyone knows why they're there
-  **Commitment** - Members actually want to help host

### Member Roles

| Role | Can Read? | Can Host? | Can Invite? |
|------|-----------|-----------|-------------|
| **Founder** | Yes | Yes | Yes |
| **Full** | Yes | Yes | No |
| **Pending** | No | No | No |
| **Observer** | Yes | No | No |

---

##  How Hosting Works

### The P2P Architecture

Cashew uses **peer-to-peer (P2P) networking** - there's no central server. When you host content, here's what happens:

```
You upload content → Cashew creates a Thing (hash ID)
                       ↓
            Thing stored on YOUR computer
                       ↓
        You create a Network and invite friends
                       ↓
         Friends download and host copies
                       ↓
       Content is now replicated across multiple nodes
                       ↓
          If you go offline, friends keep serving it!
```

### Storage Backend

**Where does data live?**

Everything is stored in your `data/` directory:

```
data/
├── storage/
│   ├── content/          # The actual files (by hash)
│   │   ├── abc123...     # Thing content
│   │   ├── def456...     # Another Thing
│   │   └── ...
│   └── metadata/         # Thing metadata (MIME types, etc.)
│       ├── abc123.json
│       └── def456.json
├── ledger/               # Append-only event log
│   └── events.db         # SQLite database
└── networks/             # Network membership data
    ├── network1.json
    └── network2.json
```

**Content Deduplication:** Same content = same hash! If two people upload identical files, they share the same hash and storage.

### How Content Serving Works

**When someone requests a Thing:**

1. **Gateway receives HTTP request:** `GET /thing/abc123...`
2. **Storage layer checks:** "Do I have this hash?"
3. **If yes:** Serve it directly from disk
4. **If no:** Ask network members ("Who has abc123...?")
5. **Download from peer:** Fetch it, verify hash, serve to user
6. **Cache locally:** Keep a copy for next time

```
  Browser              Your Node           Friend's Node
     │                     │                      │
     │  GET /thing/abc123  │                      │
     ├────────────────────►│                      │
     │                     │  "Got abc123?"       │
     │                     ├─────────────────────►│
     │                     │                      │
     │                     │   sends content      │
     │                     │◄─────────────────────┤
     │                     │                      │
     │   serves content    │                      │
     │◄────────────────────┤                      │
     │                     │                      │
```

### WebSocket Real-Time Updates

Cashew uses **WebSockets** for instant notifications:

- New content available
- Network member joined/left  
- Thing updated by network
- Reputation changes

Connect to: `ws://localhost:8080/ws`

**Example WebSocket message:**
```json
{
  "type": "thing_updated",
  "thing_hash": "abc123...",
  "network_id": "my-blog",
  "timestamp": 1738368000
}
```

---

##  Cryptography & Key System

### Node Identity

Every Cashew node has a **unique cryptographic identity**:

```
Node Identity
├── Private Key (Ed25519)  ← Keep this SECRET!
├── Public Key (Ed25519)   ← Share with friends
└── Node ID (BLAKE3 hash)  ← Your network address
```

**Ed25519 Elliptic Curve Cryptography:**
- **Fast:** Sign/verify in microseconds
- **Small:** 32-byte keys, 64-byte signatures
- **Secure:** 128-bit security level
- **Modern:** Used by Signal, SSH, TLS 1.3

### How Authentication Works

**When you join a network:**

1. **Friend sends you invitation** (signed with their key)
2. **You verify signature** (proves it's really them)
3. **You accept invitation** (sign acceptance with your key)
4. **Network records both signatures** (permanent proof)

```
Invitation Message:
{
  "network_id": "my-blog",
  "invitee": "your-node-id",
  "role": "full",
  "timestamp": 1738368000,
  "signature": "4a3b9c..."  ← Signed by inviter's private key
}

You verify:
✓ Signature valid
✓ Inviter is network member
✓ Timestamp recent

→ Accept and sign your response!
```

### Content Verification

Every Thing is **content-addressed** using **BLAKE3 hashing**:

```
Original Content → BLAKE3 Hash Function → Hash (Thing ID)

Example:
"Hello, World!" → blake3() → "abc123def456..."
```

**Why BLAKE3?**
- **Fastest cryptographic hash** (8x faster than SHA-256)
- **Secure:** Collision-resistant, preimage-resistant
- **Parallel:** Uses all CPU cores
- **Verified:** Open-source, peer-reviewed

**Verification process:**
```
Download content → Hash it → Compare to Thing ID
                                    ↓
                          Match? ✓ Valid content!
                          Mismatch? ✗ Corrupted or fake!
```

### Signature Verification Example

```cpp
// Simplified C++ code from Cashew

bool verify_invitation(const Invitation& inv) {
    // 1. Get inviter's public key from network ledger
    auto pub_key = ledger.get_member_key(inv.inviter_id);
    
    // 2. Reconstruct signed message
    auto message = serialize(inv.network_id, inv.invitee, 
                            inv.role, inv.timestamp);
    
    // 3. Verify Ed25519 signature
    return crypto::Ed25519::verify(
        message,           // What was signed
        inv.signature,     // Claimed signature  
        pub_key           // Inviter's public key
    );
}
```

---

## ⚡ Proof-of-Work & Reputation

### Why Proof-of-Work?

Cashew uses **lightweight proof-of-work** to prevent spam and build reputation. NOT for cryptocurrency!

**Goals:**
- Prevent spam (costs CPU time)
- Build trust (proves commitment)
- Prevent Sybil attacks (can't fake effort)
- No money involved (this isn't crypto!)

### How It Works

**When you want to:**
- Join a network
- Publish new content
- Invite someone

You must solve a **small computational puzzle**:

```
Goal: Find a number (nonce) such that:

  BLAKE3(message + nonce) < difficulty_target

Example:
  message = "Join network 'my-blog' as full member"
  nonce = 0, 1, 2, 3, ... (try until you find valid one)
  
  Try nonce=0:     hash = "9a2f..."  ✗ Too high
  Try nonce=1:     hash = "8d4c..."  ✗ Too high  
  Try nonce=2:     hash = "7b1e..."  ✗ Too high
  ...
  Try nonce=1847:  hash = "0003..."  ✓ Success!
```

### Difficulty Levels

```
Action                  Difficulty    Avg Time
─────────────────────────────────────────────
View content            None          0 sec
Join network (invited)  Low           ~1 sec
Publish content         Medium        ~5 sec
Create network          High          ~30 sec
Invite member           Medium        ~5 sec
```

**Difficulty = number of leading zero bits required in hash**

### Reputation System

Every action you take builds **reputation**:

```
Reputation Score
├── Proof-of-Work solved (+points)
├── Content hosted (+points per GB/day)
├── Uptime (+points per hour online)
├── Invitations honored (+points)
└── Bad behavior (-points)
```

**Why reputation matters:**
- Higher reputation → More trusted
- Trusted nodes get priority for network slots
- Reputation is network-specific (can't transfer)
- Founder always has highest reputation

**Example reputation calculation:**
```
Base: 0
+ Joined network (PoW solved): +10
+ Hosted 500MB for 7 days: +35
+ 99% uptime this week: +50
+ Invited 2 members who stayed: +20
= Total: 115 reputation points
```

### Spam Prevention

Combining **PoW + Reputation** prevents abuse:

```
Attacker wants to spam network with fake content:
1. Must solve PoW for each Thing (~5 sec each)
2. Reputation starts at 0 (low priority)
3. Other members can ignore low-reputation nodes
4. After X rejections, auto-banned
5. Creating new identity requires PoW
   → Expensive to create many fake identities!
```

---

<a name="troubleshooting"></a>
## Troubleshooting

### Common Issues

#### "Port 8080 already in use"

**Solution:** Change the port in `cashew.conf`:
```ini
http_port = 8081
```

#### "Cannot connect to http://localhost:8080"

**Check:**
1. Is Cashew node running? Look for the startup message
2. Is firewall blocking it?
3. Try http://127.0.0.1:8080 instead

#### "Storage directory not writable"

**Solution:**
```bash
# Create the data directory
mkdir -p ./data/storage

# Fix permissions (Linux/macOS)
chmod -R 755 ./data
```

#### "Node crashes on startup"

This is a known issue with Ed25519 key generation on some systems.

**Workaround:** Already implemented! The node uses a temporary ID for testing.

**Long-term fix:** Coming soon in next release.

---

<a name="faq"></a>
##  Frequently Asked Questions

### General Questions

**Q: Is Cashew cryptocurrency/blockchain?**  
A: **Nope!** No coins, no mining, no blockchain. Just content sharing.

**Q: Do I need to be online 24/7?**  
A: Not if other network members are online. That's the beauty of redundancy!

**Q: Can I make money with Cashew?**  
A: That's not the goal. Cashew is about **sharing**, not selling.

**Q: How much disk space do I need?**  
A: Depends on what you host! Start with 1GB free and go from there.

**Q: Is my data encrypted?**  
A: Content is stored as-is. Use your own encryption if needed.

### Technical Questions

**Q: What happens if all network members go offline?**  
A: The Thing becomes unavailable until someone comes back online.

**Q: Can I host NSFW content?**  
A: Technically yes, but remember: your friends are hosting it too. Be respectful!

**Q: How do I delete a Thing?**  
A: You can remove it from YOUR storage, but others in the network may still have it.

**Q: What's with the peanut and cat mascot?**  
A: Cashew is a nut! And Mona is a cat from 2channel (the OG Japanese imageboard). She's here to guide you through the decentralized web!

### Privacy Questions

**Q: Can my ISP see what I'm hosting?**  
A: They can see you're running Cashew, but not WHAT you're hosting (future encryption).

**Q: Can the government shut down Cashew?**  
A: They can't shut down a decentralized network. There's no central server!

**Q: Who can see my Node ID?**  
A: Anyone you connect to. It's public, like an IP address.

---

##  Next Steps

Congratulations! You now know the basics of Cashew! 

**Ready for more?**
1.  Read the [Advanced User Guide](ADVANCED.md)
2.  Check out the [API Documentation](API.md)
3.  Join the community (forum link TBD)
4.  Report bugs on GitHub

**Want to contribute?**
-  Submit a pull request
-  Improve documentation
-  Report issues
-  Help others in the community

---

##  Credits

```
    Cashew Network was created with love by people who believe
    the Internet should belong to everyone, not corporations.
    
    Special thanks to:
    • Mona the cat (our mascot, from 2channel)
    • All the early testers
    • The open-source community
    • You, for using Cashew!
    
       /\__/\
     （　´∀｀）"Thank you!"
     （　　　）
       ｜ ｜ |
     （＿_)＿）
```

**License:** MIT (Free as in freedom!)  
**Website:** cashew-network.org (coming soon)  
**Support:** GitHub Issues  

---

**Made with C++**

*"Freedom over profit. Privacy over surveillance."*

---

**Last Updated:** February 1, 2026  
**Version:** 0.1.0
