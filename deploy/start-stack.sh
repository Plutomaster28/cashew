#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-http}"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
RUN_DIR="$SCRIPT_DIR/run"

mkdir -p "$RUN_DIR"

CASHEW_BIN="$REPO_ROOT/build/src/cashew"
if [[ ! -x "$CASHEW_BIN" ]]; then
  echo "Cashew binary missing at build/src/cashew. Build first with cmake --build build"
  exit 1
fi

if ! command -v caddy >/dev/null 2>&1; then
  echo "Caddy not found in PATH. Install Caddy first."
  exit 1
fi

if ! command -v perl >/dev/null 2>&1; then
  echo "Perl not found in PATH. Install perl + required modules first."
  exit 1
fi

CADDY_CONFIG="$SCRIPT_DIR/Caddyfile"
if [[ "$MODE" == "https" ]]; then
  if [[ ! -f "$SCRIPT_DIR/Caddyfile.https" ]]; then
    cp "$SCRIPT_DIR/Caddyfile.https.example" "$SCRIPT_DIR/Caddyfile.https"
    echo "Created deploy/Caddyfile.https from example. Edit domains before public use."
  fi
  CADDY_CONFIG="$SCRIPT_DIR/Caddyfile.https"
fi

pkill -f "build/src/cashew" || true
pkill -f "examples/perl-cgi/server.pl" || true
pkill -x caddy || true
sleep 0.2

nohup "$CASHEW_BIN" > "$RUN_DIR/cashew.out.log" 2> "$RUN_DIR/cashew.err.log" &
echo $! > "$RUN_DIR/cashew.pid"

nohup perl "$REPO_ROOT/examples/perl-cgi/server.pl" 8000 > "$RUN_DIR/guestbook.out.log" 2> "$RUN_DIR/guestbook.err.log" &
echo $! > "$RUN_DIR/guestbook.pid"

nohup caddy run --config "$CADDY_CONFIG" > "$RUN_DIR/caddy.out.log" 2> "$RUN_DIR/caddy.err.log" &
echo $! > "$RUN_DIR/caddy.pid"

LAN_IP="$(hostname -I 2>/dev/null | awk '{print $1}')"

echo "Stack is up"
echo "Local: http://127.0.0.1/miyamii/"
if [[ -n "$LAN_IP" ]]; then
  echo "LAN:   http://$LAN_IP/miyamii/"
fi
echo "Logs: deploy/run/*.log"
echo "Stop: ./deploy/stop-stack.sh"
