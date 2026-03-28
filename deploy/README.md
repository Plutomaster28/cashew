# Public Deployment (Outside LAN)

This folder contains edge configs to expose Cashew beyond your local network.

## 1. Baseline origin setup

1. Run Cashew node on origin host (`127.0.0.1:8080` for API).
2. Optionally run frontend bridge on `127.0.0.1:8081`.
3. Keep origin services bound locally where possible.

## 2. Edge proxy choices

- Caddy: use [Caddyfile](Caddyfile) for automatic HTTPS and simple ops.
- Nginx: use [nginx.conf](nginx.conf) if you already standardize on Nginx.

## 3. Reachability from outside network

Choose one:

- Direct self-hosting:
  - Open/forward `80` and `443` from router to edge host.
  - Point DNS `A/AAAA` records to your public IP.
- Relay VPS model:
  - Keep origin behind NAT.
  - Build WireGuard tunnel to a small VPS.
  - Run edge proxy on VPS, reverse proxy over tunnel.
- Onion endpoint:
  - Expose frontend/API through Tor hidden service.

## 4. Share command with public URL

Set `public_gateway` in `cashew.conf`:

```json
{
  "public_gateway": "https://example.org",
  "http_port": 8080
}
```

Then:

```bash
./build/src/cashew share <hash>
```

It will emit internet-facing links automatically.

## 5. Minimum security checklist

1. Enforce HTTPS at edge.
2. Keep Cashew/API bound to loopback unless intentionally exposed.
3. Run firewall allow-list for management ports.
4. Backup identity and data directories regularly.
5. Validate links from an external network before announcements.
