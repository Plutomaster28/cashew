# Stub/Mock Implementation Audit

audit of all the "not actually implemented" parts in the codebase.

---

## CRITICAL - Makes Node Non-Functional

### 1. **GatewayServer - NO HTTP SERVER** 
**File:** `src/gateway/gateway_server.cpp:134-138`  
**Impact:** BREAKING - port 8080 never opens, nothing listens
```cpp
// In a real implementation, this would start an actual HTTP server
// For now, we'll simulate with a processing thread
server_thread_ = std::thread([this]() {
    process_requests();  // just sleeps forever
});
```
**Fix needed:** Implement actual HTTP server (use cpp-httplib or Boost.Beast)

---

## HIGH PRIORITY - Core Functionality Missing

### 2. **Network Sending - NO ACTUAL NETWORKING**
**Files:**
- `src/network/gossip.cpp:636` - "TODO: Actual network sending"
- `src/network/router.hpp:245` - "TODO: Actual network sending"
- `src/network/peer.cpp:460-461` - "stub"
- `src/network/router.cpp:817,824` - "TODO: Integrate with SessionManager"

**Impact:** Nodes can't communicate with each other, no P2P
**Fix needed:** Wire up SessionManager to actually send data over sockets

### 3. **Session Encryption - STUB**
**File:** `src/network/session.cpp:322-323`
```cpp
// TODO: Extract nonce from ciphertext and decrypt properly with ChaCha20Poly1305
// For now, return nullopt as placeholder
return std::nullopt;
```
**Impact:** No actual encryption happening
**Fix needed:** Implement ChaCha20-Poly1305 decrypt

### 4. **Signature Verification - EVERYWHERE**
**Files:**
- `src/network/session.cpp` - handshake signatures (lines 123, 151, 203, 235)
- `src/network/network.cpp` - invitation signatures (lines 129-130, 162)
- `src/network/router.cpp` - message signatures (lines 213, 619, 664)
- `src/core/ledger/ledger.cpp` - event signatures (lines 483, 522)
- `src/network/gossip.cpp` - announcement signatures (lines 461, 469, 497)
- `src/security/access.cpp` - token signatures (lines 340-341, 351-352)
- `src/core/reputation/reputation.cpp` - attestation signatures (lines 396, 416)
- `src/security/attack_prevention.cpp` - signature checks (lines 392-393)

**Impact:** NO CRYPTOGRAPHIC SECURITY - anyone can forge anything
**Fix needed:** Actually call Ed25519::sign() and Ed25519::verify()

### 5. **Proof-of-Work - NOT VERIFIED**
**File:** `src/security/access.cpp:466-467`
```cpp
// TODO: Verify PoW solution meets difficulty
// For now, just check it's not empty
return !solution.nonce.empty();
```
**Impact:** PoW is useless, anyone can bypass it
**Fix needed:** Actually verify BLAKE3 hash has required leading zeros

---

## MEDIUM PRIORITY - Features Incomplete

### 6. **Onion Routing - DISABLED**
**File:** `src/network/router.cpp:797-799, 809`
```cpp
// TODO: Implement actual onion routing encryption
// For now, return empty (onion routing disabled)
return {};
```
**Impact:** No anonymity features
**Fix needed:** Layer encryption for multi-hop routing

### 7. **Thing/Network Persistence - STUBS**
**Files:**
- `src/core/thing/thing.cpp:123-124, 135-136` - load/save not implemented
- `src/network/network.cpp:465, 471` - persistence stubs
- `src/network/network.cpp:326-327, 333` - serialization stubs

**Impact:** Things/networks don't persist across restarts
**Fix needed:** Actually write to storage backend

### 8. **Ledger Operations - INCOMPLETE**
**Files:**
- `src/core/ledger/state.cpp:172` - key revocation
- `src/core/ledger/state.cpp:211` - member removal
- `src/core/ledger/state.cpp:238` - thing removal
- `src/core/ledger/ledger.cpp:568` - not implemented (unknown what)
- `src/core/ledger/ledger.cpp:685-686` - chain verification stub
- `src/core/ledger/ledger.cpp:775` - conflict detection stub

**Impact:** Can't remove things, basic validation missing
**Fix needed:** Implement deletion and verification

### 9. **Decay System - NOT CONNECTED**
**Files:**
- `src/core/decay/decay.cpp:189` - "TODO: Actually remove keys through ledger"
- `src/core/decay/decay.cpp:201` - "TODO: Actually remove Thing through ledger"
- `src/core/decay/decay.cpp:390` - "TODO: Track key issuance time"

**Impact:** Decay calculations happen but don't affect anything
**Fix needed:** Wire to ledger to actually expire keys/Things

### 10. **PoStake - NOT CONNECTED**
**File:** `src/core/postake/postake.cpp:288-289`
```cpp
// TODO: Actually issue keys through ledger
// For now, just log
```
**Impact:** PoStake calculations happen but no keys issued
**Fix needed:** Connect to ledger

---

## LOW PRIORITY - Nice to Have

### 11. **Node Identity - DISABLED**
**File:** `src/main.cpp:93` (and Node initialization)
```cpp
// TODO: Fix Node initialization crash - for now create temp NodeID
```
**Impact:** No persistent identity, crashes on Ed25519 keygen
**Status:** Workaround active, temp ID works for testing
**Fix needed:** Debug libsodium crash or use alternative crypto lib

### 12. **Key Rotation - STUB**
**File:** `src/core/node/node_identity.cpp:174-175`
```cpp
// TODO: Create rotation certificate signed by old key
// For now just return new identity
```
**Impact:** Can rotate but no proof of continuity
**Fix needed:** Sign rotation cert

### 13. **Sybil Detection - BASIC**
**File:** `src/security/attack_prevention.cpp:175`
```cpp
// TODO: Implement graph-based Sybil detection
```
**Impact:** Only basic checks, no social graph analysis
**Fix needed:** Implement graph algorithms

### 14. **Storage Compaction - NOT IMPLEMENTED**
**File:** `src/storage/storage.cpp:264-265`
```cpp
CASHEW_LOG_INFO("Storage compaction not implemented for filesystem backend");
// TODO: Implement when using LevelDB/RocksDB
```
**Impact:** Storage grows forever
**Fix needed:** Implement garbage collection

### 15. **Static File Serving - STUB**
**File:** `src/gateway/gateway_server.cpp:836`
```cpp
// TODO: Serve static files from web_root
```
**Impact:** Can't serve web UI files
**Fix needed:** File serving implementation

### 16. **Peer Connection - STUB**
**File:** `src/network/peer.cpp:286-287`
```cpp
// TODO: Implement actual connection via SessionManager
// For now, this is a stub
```
**Impact:** Can't actually connect to peers
**Fix needed:** Wire SessionManager

---

## Summary

**Total stub implementations found:** 90+

**Critical (blocks basic testing):** 1
- HTTP server not running

**High priority (breaks P2P):** 5
- No network sending
- No encryption
- No signature verification
- No PoW verification
- No persistence

**Medium priority (features incomplete):** 5
- Onion routing disabled
- Ledger operations missing
- Decay/PoStake not connected

**Low priority (nice to have):** 5+
- Node identity disabled
- Various optimizations
- Advanced features

---

## What Actually Works

✅ Storage backend (filesystem)
✅ Ledger event log (append-only)
✅ Network registry (in-memory)
✅ ContentRenderer (basic)
✅ WebSocket handler (structure)
✅ Crypto primitives (BLAKE3, Ed25519, ChaCha20)
✅ PoW calculation (argon2)
✅ Reputation calculation
✅ All the classes compile and link

---

## What Doesn't Work

❌ HTTP server (nothing listens on port 8080)
❌ Actual P2P networking (no sockets)
❌ Cryptographic security (no signatures verified)
❌ PoW enforcement (not checked)
❌ Persistence (Things/networks lost on restart)
❌ Multi-hop routing
❌ Real encryption

---

## Recommended Fix Priority

1. **Implement HTTP server** - need this for ANY testing
2. **Basic socket networking** - SessionManager send/recv
3. **Signature verification** - Ed25519 actually working
4. **PoW verification** - BLAKE3 difficulty check
5. **Persistence** - Thing/network serialization to disk
6. **Everything else** - after basic P2P works

---

last updated: february 1, 2026  
status: compiled successfully, functionally incomplete
