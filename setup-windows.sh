#!/bin/bash
# Cashew Network - MSYS2/UCRT64 Dependency Setup Script
# Run this from MSYS2 UCRT64 terminal

set -e

echo "========================================="
echo "   Cashew Network - Dependency Setup     "
echo "========================================="
echo ""

# Check if we're in MSYS2 UCRT64 environment
if [[ ! "$MSYSTEM" == "UCRT64" ]]; then
    echo "Error: This script must be run from MSYS2 UCRT64 terminal"
    echo "   Please open the UCRT64 terminal (purple icon) and run this script again"
    exit 1
fi

echo "Running in MSYS2 UCRT64 environment"
echo ""

# Update package database
echo "Updating package database..."
pacman -Sy --noconfirm

# Define required packages
REQUIRED_PACKAGES=(
    "base-devel"
    "mingw-w64-ucrt-x86_64-gcc"
    "mingw-w64-ucrt-x86_64-cmake"
    "mingw-w64-ucrt-x86_64-ninja"
    "mingw-w64-ucrt-x86_64-libsodium"
    "mingw-w64-ucrt-x86_64-spdlog"
    "mingw-w64-ucrt-x86_64-nlohmann-json"
    "mingw-w64-ucrt-x86_64-gtest"
)

MISSING_PACKAGES=()

echo " Checking for required packages..."
echo ""

for package in "${REQUIRED_PACKAGES[@]}"; do
    if pacman -Qi "$package" &> /dev/null; then
        echo "  ✓ $package (installed)"
    else
        echo "  ✗ $package (missing)"
        MISSING_PACKAGES+=("$package")
    fi
done

echo ""

if [ ${#MISSING_PACKAGES[@]} -eq 0 ]; then
    echo " All required dependencies are already installed!"
else
    echo " Installing ${#MISSING_PACKAGES[@]} missing package(s)..."
    echo ""
    
    # Install missing packages
    for package in "${MISSING_PACKAGES[@]}"; do
        echo "Installing $package..."
        pacman -S --needed --noconfirm "$package"
    done
    
    echo ""
    echo " All required dependencies installed successfully!"
fi

echo ""
echo "Verifying toolchain..."
echo ""

# Verify installations
if command -v gcc &> /dev/null; then
    echo "  ✓ GCC: $(gcc --version | head -n1)"
else
    echo "  ✗ GCC not found"
    exit 1
fi

if command -v cmake &> /dev/null; then
    echo "  ✓ CMake: $(cmake --version | head -n1)"
else
    echo "  ✗ CMake not found"
    exit 1
fi

if command -v ninja &> /dev/null; then
    echo "  ✓ Ninja: $(ninja --version)"
else
    echo "  ✗ Ninja not found"
    exit 1
fi

echo ""
echo "========================================="
echo "Setup Complete!"
echo "========================================="
echo ""
echo "========================================="
echo " Setup Complete!"
echo "========================================="
echo ""
echo "Note: BLAKE3 is bundled and will be built from source"
echo ""
echo "Next steps:"
echo "  1. Configure: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release"
echo "  2. Build:     cmake --build build"
echo "  3. Run:       ./build/src/cashew_node.exe"
echo ""
echo "Happy building!"

