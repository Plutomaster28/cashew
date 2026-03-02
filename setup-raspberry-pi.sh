#!/bin/bash
# Cashew Framework - Raspberry Pi Setup Script
# Tested on Raspberry Pi OS (Debian-based)

set -e

echo "==================================="
echo "Cashew Framework - Raspberry Pi Setup"
echo "==================================="
echo ""

# Detect ARM architecture
ARCH=$(uname -m)
echo "Detected architecture: $ARCH"

if [[ ! "$ARCH" =~ ^(aarch64|arm64|armv7l|armv8)$ ]]; then
    echo "Warning: This script is optimized for ARM processors (Raspberry Pi)"
    echo "Detected: $ARCH"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""
echo "Step 1: Updating package lists..."
sudo apt-get update

echo ""
echo "Step 2: Installing build dependencies..."
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libsodium-dev \
    nlohmann-json3-dev

echo ""
echo "Step 3: Installing spdlog..."
# spdlog might not be in older Raspberry Pi OS repos, so we install from source if needed
if ! pkg-config --exists spdlog; then
    echo "spdlog not found in system packages, installing from source..."
    cd /tmp
    if [ -d "spdlog" ]; then
        rm -rf spdlog
    fi
    git clone --depth 1 --branch v1.12.0 https://github.com/gabime/spdlog.git
    cd spdlog
    mkdir -p build && cd build
    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSPDLOG_BUILD_SHARED=ON \
        -DSPDLOG_BUILD_EXAMPLE=OFF \
        -DSPDLOG_BUILD_TESTS=OFF \
        ..
    cmake --build .
    sudo cmake --install .
    cd ~
    echo "spdlog installed successfully"
else
    echo "spdlog already installed"
fi

echo ""
echo "Step 4: Configuring Cashew build..."
cd "$(dirname "$0")"
mkdir -p build
cd build

# Configure with optimizations for ARM
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCASHEW_BUILD_TESTS=ON \
    ..

echo ""
echo "Step 5: Building Cashew..."
echo "This may take several minutes on Raspberry Pi..."
cmake --build . -j$(nproc)

echo ""
echo "Step 6: Running tests..."
ctest --output-on-failure

echo ""
echo "==================================="
echo "Cashew Framework built successfully!"
echo "==================================="
echo ""
echo "Binary location: build/src/cashew"
echo ""
echo "To install system-wide:"
echo "  cd build && sudo cmake --install ."
echo ""
echo "To run:"
echo "  ./build/src/cashew"
echo ""
echo "Raspberry Pi specific notes:"
echo "  - ARM NEON SIMD optimizations enabled"
echo "  - BLAKE3 ARM acceleration active"
echo "  - Build optimized for $(nproc) CPU cores"
echo ""
