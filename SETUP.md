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
- libsodium, spdlog, nlohmann-json, openssl, gtest

Note: BLAKE3 is bundled in the repository (third_party/BLAKE3) and built from source.

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

### Windows: Missing DLL when launching cashew.exe

This typically means the UCRT64 runtime path is not in PATH for your current shell.

Use:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\start-stack.ps1
```

The startup script detects and prepends UCRT64 runtime path for that process.

### Windows: "Permission denied"

Make sure you're running from MSYS2 UCRT64 terminal and the script has execute permissions:
```bash
chmod +x setup-windows.sh
./setup-windows.sh
```

### Linux: "blake3 not found"

Cashew uses bundled BLAKE3 from third_party/BLAKE3, so a system blake3 package is not required.

### "Must be run as root" or "sudo required"

The scripts use `sudo` automatically on Linux. On Windows (MSYS2), you shouldn't need admin rights.

---

**After setup, run:**
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
