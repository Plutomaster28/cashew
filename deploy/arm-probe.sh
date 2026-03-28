#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build-linux-arm64}"

echo "ARM64 probe for Cashew"

echo "Checking toolchain..."
if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 || ! command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
  echo "Missing aarch64 cross-compiler."
  echo "Install on Ubuntu/Debian:"
  echo "  sudo apt-get update"
  echo "  sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
  exit 2
fi

cat > "$BUILD_DIR.toolchain.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
EOF

echo "Configuring ARM64 cross build..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$BUILD_DIR.toolchain.cmake" -DBUILD_TESTS=OFF

echo "Building ARM64 cross targets..."
cmake --build "$BUILD_DIR" -j

echo "ARM64 cross-build probe completed successfully."
