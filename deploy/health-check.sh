#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://127.0.0.1}"
WRITE_TEST="${2:-false}"
RETRIES="${3:-3}"

failures=0

test_endpoint() {
  local name="$1"
  local url="$2"
  local expect_pattern="$3"

  local code="000"
  local body=""
  local attempt=1
  while [[ "$attempt" -le "$RETRIES" ]]; do
    code="$(curl -s -o /tmp/cashew-health-body.txt -w "%{http_code}" "$url" || true)"
    body="$(cat /tmp/cashew-health-body.txt 2>/dev/null || true)"
    if [[ "$code" == "200" ]]; then
      break
    fi
    attempt=$((attempt + 1))
    sleep 1
  done

  local ok="false"
  if [[ "$code" == "200" ]] && [[ "$body" =~ $expect_pattern ]]; then
    ok="true"
  fi

  printf "%-18s status=%-3s success=%-5s\n" "$name" "$code" "$ok"
  if [[ "$ok" != "true" ]]; then
    failures=$((failures + 1))
  fi
}

echo ""
echo "Cashew Stack Health Check"
echo "Base URL: $BASE_URL"
echo ""

test_endpoint "Site" "$BASE_URL/miyamii/" "miyamii.net"
test_endpoint "Health API" "$BASE_URL/health" "healthy|cashew-gateway"
test_endpoint "Guestbook List" "$BASE_URL/miyamii/cgi-bin/guestbook.pl?action=list" "posts"

if [[ "$WRITE_TEST" == "true" ]]; then
  stamp="$(date +%s)"
  post_code="$(curl -s -o /tmp/cashew-health-post.txt -w "%{http_code}" -X POST "$BASE_URL/miyamii/cgi-bin/guestbook.pl" -H "Content-Type: application/x-www-form-urlencoded" --data "name=healthcheck&subject=probe&body=health+probe+$stamp" || true)"
  post_body="$(cat /tmp/cashew-health-post.txt 2>/dev/null || true)"
  post_ok="false"
  if [[ "$post_code" == "200" ]] && [[ "$post_body" =~ "ok" ]]; then
    post_ok="true"
  fi
  printf "%-18s status=%-3s success=%-5s\n" "Guestbook Post" "$post_code" "$post_ok"
  if [[ "$post_ok" != "true" ]]; then
    failures=$((failures + 1))
  fi

  verify_code="$(curl -s -o /tmp/cashew-health-verify.txt -w "%{http_code}" "$BASE_URL/miyamii/cgi-bin/guestbook.pl?action=list" || true)"
  verify_body="$(cat /tmp/cashew-health-verify.txt 2>/dev/null || true)"
  verify_ok="false"
  if [[ "$verify_code" == "200" ]] && [[ "$verify_body" =~ "health probe $stamp" ]]; then
    verify_ok="true"
  fi
  printf "%-18s status=%-3s success=%-5s\n" "Guestbook Verify" "$verify_code" "$verify_ok"
  if [[ "$verify_ok" != "true" ]]; then
    failures=$((failures + 1))
  fi
fi

echo ""
if [[ "$failures" -gt 0 ]]; then
  echo "FAILED checks: $failures"
  exit 1
fi

echo "All checks passed."

if command -v curl >/dev/null 2>&1; then
  public_ip="$(curl -s https://api.ipify.org || true)"
  if [[ -n "$public_ip" ]]; then
    echo "Share URL (public IP): http://$public_ip/miyamii/"
  fi
fi
