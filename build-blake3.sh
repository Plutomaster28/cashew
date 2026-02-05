#!/bin/bash
# Download and build BLAKE3 for MSYS2/UCRT64
# Run this if blake3 is not available in MSYS2 repos

set -e

echo "========================================="
echo " Building BLAKE3 from source"
echo "========================================="
echo ""

# Check if we're in MSYS2 UCRT64 environment
if [[ ! "$MSYSTEM" == "UCRT64" ]]; then
    echo " Error: This script must be run from MSYS2 UCRT64 terminal"
    exit 1
fi

# Create temp directory
TEMP_DIR="/tmp/blake3-build"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

echo " Cloning BLAKE3 repository..."
git clone --depth 1 https://github.com/BLAKE3-team/BLAKE3.git
cd BLAKE3/c

echo ""
echo " Building BLAKE3..."

# Build static library
gcc -O3 -march=native -c blake3.c blake3_dispatch.c blake3_portable.c \
    blake3_sse2_x86-64_windows_gnu.S blake3_sse41_x86-64_windows_gnu.S \
    blake3_avx2_x86-64_windows_gnu.S blake3_avx512_x86-64_windows_gnu.S

ar rcs libblake3.a blake3.o blake3_dispatch.o blake3_portable.o \
    blake3_sse2_x86-64_windows_gnu.o blake3_sse41_x86-64_windows_gnu.o \
    blake3_avx2_x86-64_windows_gnu.o blake3_avx512_x86-64_windows_gnu.o

echo ""
echo " Installing BLAKE3..."

# Install to UCRT64 directories
INSTALL_PREFIX="/ucrt64"
sudo mkdir -p "$INSTALL_PREFIX/include/blake3"
sudo mkdir -p "$INSTALL_PREFIX/lib"

sudo cp blake3.h "$INSTALL_PREFIX/include/blake3/"
sudo cp libblake3.a "$INSTALL_PREFIX/lib/"

echo ""
echo " BLAKE3 built and installed successfully!"
echo ""
echo "Location:"
echo "  Headers: $INSTALL_PREFIX/include/blake3/blake3.h"
echo "  Library: $INSTALL_PREFIX/lib/libblake3.a"
echo ""
echo "You can now build Cashew normally."

# Cleanup
cd /
rm -rf "$TEMP_DIR"
