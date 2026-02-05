# Cashew Network - Dependency Setup Scripts

This directory contains automated setup scripts to install build dependencies.

## Windows (MSYS2/UCRT64)

**Requirements:** MSYS2 installed with UCRT64 environment

```bash
# Open MSYS2 UCRT64 terminal
./setup-windows.sh
```

This will:
- Verify you're in the UCRT64 environment
- Update package database
- Check for missing dependencies
- Install any missing packages automatically
- Verify the toolchain

**Installed packages:**
- base-devel (build tools)
- gcc, cmake, ninja
- libsodium, spdlog, nlohmann-json, blake3, gtest

## Linux

**Supported distributions:**
- Debian/Ubuntu/Linux Mint (apt)
- Arch/Manjaro (pacman)
- Fedora/RHEL/Rocky/AlmaLinux (dnf)
- openSUSE (zypper)

```bash
# Make executable
chmod +x setup-linux.sh

# Run
./setup-linux.sh
```

This will:
- Auto-detect your distribution
- Install appropriate packages using your package manager
- Verify the toolchain
- Check for all required libraries

## Manual Installation

If the scripts don't work for your system, see [BUILD.md](BUILD.md) for manual installation instructions.

## Troubleshooting

### Windows: "target not found: mingw-w64-ucrt-x86_64-blake3"

BLAKE3 is not yet available in MSYS2 repositories. The build system will automatically handle this. You have two options:

1. **Use bundled implementation** (automatic - recommended)
   - The project will build with a bundled BLAKE3 implementation
   - No action needed

2. **Build BLAKE3 from source:**
   ```bash
   ./build-blake3.sh
   ```
   This will download, compile, and install BLAKE3 to your UCRT64 environment.

### Windows: "Permission denied"

Make sure you're running from MSYS2 UCRT64 terminal and the script has execute permissions:
```bash
chmod +x setup-windows.sh
./setup-windows.sh
```

### Linux: "blake3 not found"

Some distributions don't have blake3 in their repositories yet. You can:
1. Build from source: https://github.com/BLAKE3-team/BLAKE3
2. The build will attempt to find it via CMake

### "Must be run as root" or "sudo required"

The scripts use `sudo` automatically on Linux. On Windows (MSYS2), you shouldn't need admin rights.

---

**🥜 After setup, run:**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
