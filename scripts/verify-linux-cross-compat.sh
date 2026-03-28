#!/bin/bash
# Verify Cashew compiles on Linux x86_64 and Linux ARM64 (cross-compile)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
X86_BUILD_DIR="$ROOT_DIR/build-x86"
ARM_BUILD_DIR="$ROOT_DIR/build-arm64"
ARM_TOOLCHAIN_FILE="$ROOT_DIR/arm64-toolchain.cmake"

printf "\n== Cashew Linux Cross-Compatibility Check ==\n"
printf "Project: %s\n" "$ROOT_DIR"

printf "\n[1/4] Native Linux x86_64 configure/build/test\n"
cmake -S "$ROOT_DIR" -B "$X86_BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
cmake --build "$X86_BUILD_DIR" -j"$(nproc)"
ctest --test-dir "$X86_BUILD_DIR" --output-on-failure

if [[ -f "$X86_BUILD_DIR/src/cashew" ]]; then
    printf "x86_64 binary: %s\n" "$X86_BUILD_DIR/src/cashew"
else
    echo "x86_64 build completed, but expected binary not found at $X86_BUILD_DIR/src/cashew"
    exit 1
fi

printf "\n[2/4] Checking ARM64 cross-compilation prerequisites\n"
if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 || ! command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
    cat << 'EOF'
Missing ARM64 cross-compilers.

Install prerequisites first:
  ./setup-linux.sh --cross-arm64

Then rerun:
  ./scripts/verify-linux-cross-compat.sh
EOF
    exit 2
fi

PKG_CONFIG_BIN="pkg-config"
if command -v aarch64-linux-gnu-pkg-config >/dev/null 2>&1; then
    PKG_CONFIG_BIN="aarch64-linux-gnu-pkg-config"
fi

if [[ "$PKG_CONFIG_BIN" == "pkg-config" ]]; then
    PKG_CONFIG_BIN="$(command -v pkg-config)"
fi

ARM_PKG_CONFIG_LIBDIR="/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig"

cat > "$ARM_TOOLCHAIN_FILE" << 'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu /usr)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

printf "\n[3/4] ARM64 configure/build (cross-compile)\n"
rm -rf "$ARM_BUILD_DIR"
PKG_CONFIG_LIBDIR="$ARM_PKG_CONFIG_LIBDIR" cmake -S "$ROOT_DIR" -B "$ARM_BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCASHEW_BUILD_TESTS=OFF \
    -DCMAKE_TOOLCHAIN_FILE="$ARM_TOOLCHAIN_FILE" \
    -DPKG_CONFIG_EXECUTABLE="$PKG_CONFIG_BIN"

cmake --build "$ARM_BUILD_DIR" -j"$(nproc)"

if [[ -f "$ARM_BUILD_DIR/src/cashew" ]]; then
    printf "ARM64 binary: %s\n" "$ARM_BUILD_DIR/src/cashew"
    file "$ARM_BUILD_DIR/src/cashew" || true
else
    echo "ARM64 build completed, but expected binary not found at $ARM_BUILD_DIR/src/cashew"
    exit 1
fi

printf "\n[4/4] Result\n"
printf "PASS: Linux x86_64 and ARM64 compilation paths validated.\n"
