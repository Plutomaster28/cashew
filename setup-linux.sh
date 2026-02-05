#!/bin/bash
# Cashew Network - Linux Dependency Setup Script
# Supports: Debian/Ubuntu, Arch, Fedora/RHEL

set -e

echo "========================================="
echo "   Cashew Network - Dependency Setup     "
echo "========================================="
echo ""

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    echo "Cannot detect Linux distribution"
    exit 1
fi

echo "Detected distribution: $DISTRO"
echo ""

# Check if running as root (needed for some package managers)
if [[ $EUID -ne 0 ]]; then
    SUDO="sudo"
    echo "Note: Will use 'sudo' for package installation"
else
    SUDO=""
fi

echo ""
echo "Installing dependencies..."
echo ""

case $DISTRO in
    ubuntu|debian|linuxmint)
        echo "Using apt package manager..."
        $SUDO apt update
        
        PACKAGES=(
            build-essential
            cmake
            ninja-build
            pkg-config
            libsodium-dev
            libspdlog-dev
            nlohmann-json3-dev
            libgtest-dev
        )
        
        # Note: blake3 is bundled, no need to install
        $SUDO apt install -y "${PACKAGES[@]}"
        ;;
        
    arch|manjaro|msys)
        echo "Using pacman package manager..."
        $SUDO pacman -Sy --needed --noconfirm \
            base-devel \
            cmake \
            ninja \
            libsodium \
            spdlog \
            nlohmann-json \
            blake3 \
            gtest
        ;;
        
    fedora|rhel|centos|rocky|almalinux)
        echo "Using dnf package manager..."
        $SUDO dnf install -y \
            gcc-c++ \
            cmake \
            ninja-build \
            libsodium-devel \
            spdlog-devel \
            json-devel \
            gtest-devel \
            blake3-devel
        ;;
        
    opensuse*|sles)
        echo "Using zypper package manager..."
        $SUDO zypper install -y \
            gcc-c++ \
            cmake \
            ninja \
            libsodium-devel \
            spdlog-devel \
            nlohmann_json-devel \
            gtest
        echo "Warning: You may need to install blake3 manually"
        ;;
        
    *)
        echo "Unsupported distribution: $DISTRO"
        echo ""
        echo "Please install the following packages manually:"
        echo "  - C++ compiler (GCC 11+ or Clang 14+)"
        echo "  - CMake 3.20+"
        echo "  - Ninja build system"
        echo "  - libsodium"
        echo "  - spdlog"
        echo "  - nlohmann-json"
        echo "  - blake3"
        echo "  - gtest"
        exit 1
        ;;
esac

echo ""
echo "Verifying toolchain..."
echo ""

# Verify installations
if command -v g++ &> /dev/null; then
    echo "  ✓ G++: $(g++ --version | head -n1)"
elif command -v clang++ &> /dev/null; then
    echo "  ✓ Clang++: $(clang++ --version | head -n1)"
else
    echo "  ✗ C++ compiler not found"
    exit 1
fi

if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1)
    echo "  ✓ CMake: $CMAKE_VERSION"
    
    # Check version is at least 3.20
    CMAKE_VER=$(cmake --version | grep -oP '(?<=version )\d+\.\d+' | head -n1)
    if [ "$(printf '%s\n' "3.20" "$CMAKE_VER" | sort -V | head -n1)" != "3.20" ]; then
        echo "  Warning: CMake version is older than 3.20"
    fi
else
    echo "  ✗ CMake not found"
    exit 1
fi

if command -v ninja &> /dev/null; then
    echo "  ✓ Ninja: $(ninja --version)"
else
    echo "    Ninja not found (optional, but recommended)"
fi

# Check for libraries
echo ""
echo "Checking libraries..."
echo ""

pkg-config --exists libsodium && echo "  ✓ libsodium" || echo "  ✗ libsodium"
pkg-config --exists spdlog && echo "  ✓ spdlog" || echo "  Warning: spdlog (may not support pkg-config)"
pkg-config --exists nlohmann_json && echo "  ✓ nlohmann-json" || echo "  Warning: nlohmann-json (may not support pkg-config)"
echo "  ✓ blake3 (bundled)"

echo ""
echo "========================================="
echo "Setup Complete!"
echo "========================================="
echo ""
echo "Next steps:"
echo "  1. Configure: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release"
echo "  2. Build:     cmake --build build"
echo "  3. Run:       ./build/src/cashew_node"
echo "  4. Test:      cd build && ctest --verbose"
echo ""
echo "Happy building!"
