#!/bin/bash
# Cashew Network - Linux Dependency Setup Script
# Supports: Debian/Ubuntu, Arch, Fedora/RHEL

set -e

ENABLE_ARM64_CROSS=0

for arg in "$@"; do
    case "$arg" in
        --cross-arm64)
            ENABLE_ARM64_CROSS=1
            ;;
        -h|--help)
            echo "Usage: ./setup-linux.sh [--cross-arm64]"
            echo ""
            echo "Options:"
            echo "  --cross-arm64   Install ARM64 cross-compilation toolchain (Linux host)"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help for usage."
            exit 1
            ;;
    esac
done

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
if [[ $ENABLE_ARM64_CROSS -eq 1 ]]; then
    echo "ARM64 cross-compilation setup: enabled"
fi
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

        if [[ $ENABLE_ARM64_CROSS -eq 1 ]]; then
            echo ""
            echo "Installing ARM64 cross-compilation packages..."

            # Ubuntu arm64 packages are hosted on ports.ubuntu.com; scope existing
            # sources to amd64 and add a dedicated arm64 sources file when needed.
            if [[ "$DISTRO" == "ubuntu" ]] && [[ -f /etc/apt/sources.list.d/ubuntu.sources ]]; then
                if [[ ! -f /etc/apt/sources.list.d/ubuntu-arm64-ports.sources ]]; then
                    echo "Configuring Ubuntu amd64/arm64 apt sources..."
                    UBUNTU_CODENAME="${VERSION_CODENAME:-noble}"
                    $SUDO cp /etc/apt/sources.list.d/ubuntu.sources /etc/apt/ubuntu.sources.bak-cashew-cross

                    cat << 'EOF' | $SUDO tee /etc/apt/sources.list.d/ubuntu.sources >/dev/null
Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: UBUNTU_RELEASE UBUNTU_RELEASE-updates UBUNTU_RELEASE-backports
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu/
Suites: UBUNTU_RELEASE-security
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

                    $SUDO sed -i "s/UBUNTU_RELEASE/$UBUNTU_CODENAME/g" /etc/apt/sources.list.d/ubuntu.sources

                    cat << 'EOF' | $SUDO tee /etc/apt/sources.list.d/ubuntu-arm64-ports.sources >/dev/null
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: UBUNTU_RELEASE UBUNTU_RELEASE-updates UBUNTU_RELEASE-backports
Components: main universe restricted multiverse
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: UBUNTU_RELEASE-security
Components: main universe restricted multiverse
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF

                    $SUDO sed -i "s/UBUNTU_RELEASE/$UBUNTU_CODENAME/g" /etc/apt/sources.list.d/ubuntu-arm64-ports.sources
                fi
            fi

            $SUDO dpkg --add-architecture arm64
            $SUDO apt update

            CROSS_PACKAGES=(
                gcc-aarch64-linux-gnu
                g++-aarch64-linux-gnu
                libc6-dev-arm64-cross
                libsodium-dev:arm64
                libspdlog-dev:arm64
                nlohmann-json3-dev:arm64
                libssl-dev:arm64
            )

            $SUDO apt install -y "${CROSS_PACKAGES[@]}"
        fi
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
        if [[ $ENABLE_ARM64_CROSS -eq 1 ]]; then
            echo ""
            echo "Warning: Automatic ARM64 cross-toolchain install is only implemented for apt-based distros."
            echo "Install ARM64 cross toolchain and target libraries manually for your distribution."
        fi
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
        if [[ $ENABLE_ARM64_CROSS -eq 1 ]]; then
            echo ""
            echo "Warning: Automatic ARM64 cross-toolchain install is only implemented for apt-based distros."
            echo "Install ARM64 cross toolchain and target libraries manually for your distribution."
        fi
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
        if [[ $ENABLE_ARM64_CROSS -eq 1 ]]; then
            echo ""
            echo "Warning: Automatic ARM64 cross-toolchain install is only implemented for apt-based distros."
            echo "Install ARM64 cross toolchain and target libraries manually for your distribution."
        fi
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

if [[ $ENABLE_ARM64_CROSS -eq 1 ]]; then
    if command -v aarch64-linux-gnu-g++ &> /dev/null; then
        echo "  ✓ ARM64 C++ cross-compiler: $(aarch64-linux-gnu-g++ --version | head -n1)"
    else
        echo "  ✗ ARM64 C++ cross-compiler not found"
        exit 1
    fi

    if command -v aarch64-linux-gnu-pkg-config &> /dev/null; then
        echo "  ✓ ARM64 pkg-config wrapper: available"
    else
        echo "  Note: aarch64-linux-gnu-pkg-config not found (standard pkg-config will be used with ARM64 libdir)"
    fi
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
if [[ $ENABLE_ARM64_CROSS -eq 1 ]]; then
    echo "  5. Verify x86 + ARM64: ./scripts/verify-linux-cross-compat.sh"
fi
echo ""
echo "Happy building!"

