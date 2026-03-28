# Public Deployment (Outside LAN)

This folder contains edge configs to expose your miyamii site publicly.

Current runtime model:

- Cashew static + API + ws: `127.0.0.1:8080`
- Perl guestbook backend: `127.0.0.1:8000`
- Public edge: Caddy on port `80`

## LAN startup answer (same process or not)

Yes, for LAN usage it is the same core startup process: run Cashew + guestbook + Caddy, then open `/miyamii/`.

- Use this when everyone is on the same network or using the host machine.
- Only add tunnel/domain steps when you need internet-wide access.

## Fast startup (recommended)

Windows (repo root):

```powershell
.\deploy\start-stack.ps1
```

If PowerShell blocks scripts in your terminal session:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\deploy\start-stack.ps1
```

Stop everything:

```powershell
.\deploy\stop-stack.ps1
```

Health check:

```powershell
.\deploy\health-check.ps1
```

Linux:

```bash
./deploy/start-stack.sh
./deploy/stop-stack.sh
./deploy/health-check.sh
```

## Manual process startup (for debugging)

Start these from repo root in separate terminals.

1. Cashew gateway/node:

```powershell
.\build\src\cashew.exe
```

2. Guestbook backend:

```powershell
perl .\examples\perl-cgi\server.pl 8000
```

3. Caddy edge:

```powershell
caddy run --config .\deploy\Caddyfile
```

Manual stop:

1. Press `Ctrl+C` in each terminal.
2. Or use `.\deploy\stop-stack.ps1` to stop known process names.

## 1. Start all origin services

In separate terminals from repo root:

```powershell
.\build\src\cashew.exe
```

```powershell
perl .\examples\perl-cgi\server.pl 8000
```

Quick start (recommended):

Windows:

```powershell
.\deploy\start-stack.ps1
```

If PowerShell blocks script execution, run with a process-scope bypass:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\start-stack.ps1
```

Linux:

```bash
./deploy/start-stack.sh
```

If needed, make scripts executable once:

```bash
chmod +x ./deploy/start-stack.sh ./deploy/stop-stack.sh
```

Stop scripts:

Windows: `./deploy/stop-stack.ps1`
Linux: `./deploy/stop-stack.sh`

Quick tunnel for festival sharing (no DNS required):

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\start-tunnel.ps1
```

This prints a temporary public URL ending in `trycloudflare.com`.

Stop tunnel:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\stop-tunnel.ps1
```

## 2. Start Caddy edge

```powershell
caddy run --config .\deploy\Caddyfile
```

Routing is already configured:

- `/miyamii/cgi-bin/guestbook.pl*` -> `127.0.0.1:8000`
- everything else -> `127.0.0.1:8080`

## 2b. HTTPS + custom domain (recommended)

Use [Caddyfile.https.example](Caddyfile.https.example) for automatic TLS.

1. Copy file and set your real domain(s):

```powershell
Copy-Item .\deploy\Caddyfile.https.example .\deploy\Caddyfile.https -Force
```

2. Edit `deploy/Caddyfile.https` and replace `miyamii.net, www.miyamii.net` with your domain.

3. Run Caddy with HTTPS config:

```powershell
caddy run --config .\deploy\Caddyfile.https
```

Caddy will request certificates automatically once DNS + ports are correct.

## 3. Test from another device on same Wi-Fi

Use your LAN URL:

```text
http://192.168.1.238/miyamii/
```

If this works, your app stack is correct.

## 4. Make it reachable by friends on the internet

1. Router port-forward: external `80` -> `192.168.1.238:80`
2. Router port-forward: external `443` -> `192.168.1.238:443` (required for HTTPS)
3. Windows firewall: allow inbound TCP `80` and `443`
4. Share your public IP URL (temporary): `http://<your-public-ip>/miyamii/`

Optional custom domain:

1. Set DNS `A` record to your public IP
2. Switch to domain-based Caddy config with TLS
3. Open/forward `443` as well

## 4b. Windows Firewall commands (Administrator terminal)

Open PowerShell as Administrator, then run:

```powershell
netsh advfirewall firewall add rule name="Cashew Public HTTP 80" dir=in action=allow protocol=TCP localport=80
netsh advfirewall firewall add rule name="Cashew Public HTTPS 443" dir=in action=allow protocol=TCP localport=443
```

Verify rules:

```powershell
netsh advfirewall firewall show rule name="Cashew Public HTTP 80"
netsh advfirewall firewall show rule name="Cashew Public HTTPS 443"
```

## 5. Keep share links correct

Set `public_gateway` in [cashew.conf](../cashew.conf) to your public URL so `cashew share` prints links that others can open.

## 6. Linux-side parity and build test from Windows

If you have WSL installed, you can sanity-check Linux compilation directly from Windows:

```powershell
wsl bash -lc "cd /mnt/c/Users/theni/OneDrive/Documents/cashew && ./setup-linux.sh"
wsl bash -lc "cd /mnt/c/Users/theni/OneDrive/Documents/cashew && cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release"
wsl bash -lc "cd /mnt/c/Users/theni/OneDrive/Documents/cashew && cmake --build build-linux -j"
```

That validates Linux toolchain/build behavior without leaving your Windows box.

## 7. ARM architecture probe (Linux)

Use the probe script on Linux/WSL to validate ARM64 cross-build capability:

```bash
chmod +x ./deploy/arm-probe.sh
./deploy/arm-probe.sh
```

If cross-compilers are missing, the script prints install commands and exits with code 2.
