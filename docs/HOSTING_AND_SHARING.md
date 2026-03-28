# Cashew Hosting and Sharing Guide

This guide explains how to host content with Cashew, share it, and expose it to the public web while preserving the project goals:

- no hard dependency on a single provider
- strong content integrity guarantees
- optional privacy-preserving access paths

## 1. What Cashew already gives you

Cashew node provides:

- content-addressed storage (BLAKE3 hash IDs)
- HTTP gateway API (`/api/thing/<hash>`, `/api/status`, `/api/networks`)
- signed identity and invitation-based network model
- local sharing CLI (`content add`, `content list`, `share`)

This means hosting is split into two layers:

1. Cashew origin node (stores and verifies content)
2. Edge exposure layer (how people reach your origin over internet)

## 2. Quick local and LAN hosting

Start node:

```bash
./build/src/cashew node
```

Add content:

```bash
./build/src/cashew content add ./my-site/index.html
./build/src/cashew content add ./my-site/style.css
./build/src/cashew content add ./my-site/app.js
```

Share links:

```bash
./build/src/cashew share <hash> --gateway http://192.168.1.20:8080
```

For LAN demos, this is enough.

## 3. Public web serving method A (self-hosted direct)

Use this when you want maximum independence from third-party platforms.

1. Keep Cashew on private host (home server, mini PC, or VPS).
2. Forward public ports from router/firewall to your host.
3. Put a reverse proxy in front of Cashew for TLS and sane headers.
4. Expose only proxy ports `80/443`; keep Cashew internal.

Example Caddyfile:

```caddy
example.org {
    encode zstd gzip

    @api path /api/* /health /ws*
    handle @api {
        reverse_proxy 127.0.0.1:8080
    }

    handle {
        reverse_proxy 127.0.0.1:8081
    }
}
```

Notes:

- `8080` is Cashew API gateway
- `8081` can be your browser-friendly frontend (for example the Perl CGI bridge)
- run Caddy on same machine or a private edge host

## 4. Public web serving method B (privacy-first onion endpoint)

Use this when you want stronger origin privacy.

1. Keep Cashew and frontend bound locally.
2. Run Tor service on same host.
3. Publish hidden service that maps to local proxy or frontend.

Minimal `torrc` snippet:

```text
HiddenServiceDir /var/lib/tor/cashew_site/
HiddenServicePort 80 127.0.0.1:8081
```

Then share the generated `.onion` URL.

Benefits:

- no router port-forward required
- hides origin IP from viewers
- avoids lock-in to single cloud edge vendor

## 5. Public web serving method C (portable relay without lock-in)

If home NAT is restrictive, run a small relay VPS you control:

1. Create WireGuard tunnel from origin host to relay VPS.
2. Expose TLS on relay (`80/443`).
3. Reverse proxy over WireGuard private IP to origin Cashew/frontend.

This preserves migration freedom because you can move relays between providers with no app-level rewrites.

## 6. Sharing model and advertised features

Cashew links are content hashes, so sharing is immutable by design:

- URL shape: `/api/thing/<blake3_hash>`
- hash mismatch means corrupted or substituted content
- invitation-based networking controls replication trust domain
- WebSocket endpoint can provide near-real-time updates (`/ws`)

Recommended share patterns:

- direct API links for technical users
- frontend landing page that references hash URLs for browser users
- release manifest (`csv` or `json`) listing asset names and hashes

## 7. Production checklist

1. Build release: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
2. Compile: `cmake --build build --parallel`
3. Run tests: `ctest --test-dir build --output-on-failure`
4. Start node: `./build/src/cashew node`
5. Add content and verify all hashes
6. Place reverse proxy in front with TLS
7. Validate from an external network (not same LAN)
8. Back up `data/` and `cashew_identity.dat`

## 8. Security baseline

- keep Cashew behind reverse proxy; do not expose debug or admin internals
- enforce HTTPS at edge
- keep identity key file backed up and access-restricted
- use firewall allow-lists where possible
- monitor request spikes and tune rate limits

## 9. What this does not solve yet

Current source still has some advanced areas marked for future implementation (for example deeper chain verification and some optional hardware key backends). These do not block the core hosting/share workflow above, but should be tracked as part of production hardening.
