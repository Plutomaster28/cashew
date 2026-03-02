#!/bin/bash
# Architecture Detection Test
# This script shows what compiler optimizations Cashew will use

echo "==================================="
echo "Cashew Architecture Detection Test"
echo "==================================="
echo ""

# Get system info
ARCH=$(uname -m)
OS=$(uname -s)

echo "System Information:"
echo "  OS: $OS"
echo "  Architecture: $ARCH"
echo ""

# Determine what optimizations would be used
echo "Compiler Optimizations:"
if [[ "$ARCH" =~ ^(aarch64|arm64|ARM64|armv8)$ ]]; then
    echo "  [*] ARM 64-bit detected"
    echo "  Flags: -O3 -mcpu=native"
    echo "  SIMD: ARM NEON (implicit)"
    echo "  BLAKE3: ARM crypto extensions (if available)"
elif [[ "$ARCH" =~ ^(arm|armv7)$ ]]; then
    echo "  [*] ARM 32-bit detected"
    echo "  Flags: -O3 -mcpu=native -mfpu=neon"
    echo "  SIMD: ARM NEON (explicit)"
    echo "  BLAKE3: NEON-optimized"
elif [[ "$ARCH" =~ ^(x86_64|amd64|AMD64)$ ]]; then
    echo "  [*] x86_64 (64-bit) detected"
    echo "  Flags: -O3 -march=native"
    echo "  SIMD: SSE2/SSE4.1/AVX2/AVX512 (based on CPU)"
    echo "  BLAKE3: x86 assembly optimizations"
elif [[ "$ARCH" =~ ^(i686|x86|X86)$ ]]; then
    echo "  [*] x86 (32-bit) detected"
    echo "  Flags: -O3 -march=native"
    echo "  SIMD: SSE2/SSE4.1 (based on CPU)"
    echo "  BLAKE3: x86 intrinsics"
else
    echo "  [!] Unknown architecture: $ARCH"
    echo "  Flags: -O3 (generic)"
    echo "  SIMD: None (portable C)"
    echo "  BLAKE3: Portable implementation"
fi

echo ""
echo "CPU Features:"

if [[ "$OS" == "Linux" ]]; then
    if [ -f /proc/cpuinfo ]; then
        if [[ "$ARCH" =~ ^(aarch64|arm64|ARM64|armv8|arm|armv7)$ ]]; then
            # ARM features
            FEATURES=$(cat /proc/cpuinfo | grep -m1 "Features" | cut -d: -f2)
            echo "  Features:$FEATURES"
            
            if cat /proc/cpuinfo | grep -q "neon"; then
                echo "  [*] NEON SIMD available"
            fi
            if cat /proc/cpuinfo | grep -q "aes"; then
                echo "  [*] ARM Crypto Extensions available"
            fi
        else
            # x86 features
            if cat /proc/cpuinfo | grep -q "sse2"; then
                echo "  [*] SSE2 available"
            fi
            if cat /proc/cpuinfo | grep -q "sse4_1"; then
                echo "  [*] SSE4.1 available"
            fi
            if cat /proc/cpuinfo | grep -q "avx2"; then
                echo "  [*] AVX2 available"
            fi
            if cat /proc/cpuinfo | grep -q "avx512"; then
                echo "  [*] AVX512 available"
            fi
        fi
    fi
elif [[ "$OS" == "Darwin" ]]; then
    # macOS
    if [[ "$ARCH" == "arm64" ]]; then
        echo "  Apple Silicon (M1/M2/M3)"
        echo "  [*] NEON SIMD (always available)"
        echo "  [*] ARM Crypto Extensions (always available)"
    else
        sysctl -a | grep machdep.cpu.features | head -1
    fi
fi

echo ""
echo "Raspberry Pi Detection:"
if [ -f /proc/device-tree/model ]; then
    MODEL=$(cat /proc/device-tree/model)
    echo "  Model: $MODEL"
    
    # Temperature (if available)
    if command -v vcgencmd &> /dev/null; then
        TEMP=$(vcgencmd measure_temp)
        echo "  $TEMP"
    fi
else
    echo "  Not a Raspberry Pi"
fi

echo ""
echo "Build Recommendation:"
if [[ "$ARCH" =~ ^(aarch64|arm64|ARM64|armv8|arm|armv7)$ ]]; then
    if [ -f /proc/device-tree/model ]; then
        echo "  Use: ./setup-raspberry-pi.sh"
    else
        echo "  Use: Standard Linux build process"
    fi
    
    if [[ ! "$ARCH" =~ ^(aarch64|arm64|ARM64)$ ]]; then
        echo "  [TIP] Consider upgrading to 64-bit OS for better performance"
    fi
else
    echo "  Use: Standard build process for your platform"
fi

echo ""
echo "To build with these optimizations:"
echo "  mkdir build && cd build"
echo "  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .."
echo "  ninja"
echo ""
echo "==================================="
