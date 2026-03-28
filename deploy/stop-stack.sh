#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUN_DIR="$SCRIPT_DIR/run"

for name in cashew guestbook caddy; do
  pid_file="$RUN_DIR/$name.pid"
  if [[ -f "$pid_file" ]]; then
    pid="$(cat "$pid_file" || true)"
    if [[ -n "$pid" ]] && kill -0 "$pid" >/dev/null 2>&1; then
      kill "$pid" || true
      echo "Stopped $name (PID $pid)"
    fi
    rm -f "$pid_file"
  fi
done

pkill -f "build/src/cashew" || true
pkill -f "examples/perl-cgi/server.pl" || true
pkill -x caddy || true

echo "Stack stop complete"
