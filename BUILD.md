# Building Cashew 🥜

## Quick Setup (Automated)

We provide setup scripts that automatically install all dependencies:

### Windows (MSYS2 UCRT64)
```bash
# Open MSYS2 UCRT64 terminal
./setup-windows.sh
```

### Linux
```bash
chmod +x setup-linux.sh
./setup-linux.sh
```

See [SETUP.md](SETUP.md) for details.

---

## Manual Setup

### Prerequisites

### Windows (MSYS2/UCRT64)
- MSYS2 with UCRT64 environment
- CMake 3.20+
- Ninja build system
- GCC toolchain (via MSYS2)

### Linux
- GCC 11+ or Clang 14+
- CMake 3.20+
- Ninja (optional but recommended)
- Package manager (apt/dnf/pacman)

## Setting up MSYS2 (Windows)

1. **Download and install MSYS2** from https://www.msys2.org/

2. **Open MSYS2 UCRT64 terminal** (the purple icon)

3. **Update package database:**
   ```bash
   pacman -Syu
   ```

4. **Install build tools and dependencies:**
   ```bash
   pacman -S --needed base-devel \
     mingw-w64-ucrt-x86_64-gcc \
     mingw-w64-ucrt-x86_64-cmake \
     mingw-w64-ucrt-x86_64-ninja \
     mingw-w64-ucrt-x86_64-libsodium \
     mingw-w64-ucrt-x86_64-spdlog \
     mingw-w64-ucrt-x86_64-nlohmann-json \
     mingw-w64-ucrt-x86_64-blake3 \
     mingw-w64-ucrt-x86_64-gtest
   ```

5. **Add UCRT64 to your PATH** (optional, for running outside MSYS2):
   - Add `C:\msys64\ucrt64\bin` to your Windows PATH environment variable

## Building

### Windows (MSYS2 UCRT64)

```bash
# Open MSYS2 UCRT64 terminal
# Navigate to project directory
cd /c/Users/YourName/path/to/cashew

# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON

# Build
cmake --build build

# Run
./build/src/cashew_node.exe
```

### Windows (PowerShell/CMD with UCRT64 in PATH)

```powershell
# Make sure C:\msys64\ucrt64\bin is in your PATH
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
cmake --build build
.\build\src\cashew_node.exe
```

### Linux (Debian/Ubuntu)

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake ninja-build \
  libsodium-dev libspdlog-dev nlohmann-json3-dev \
  libblake3-dev libgtest-dev

# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON

# Build
cmake --build build

# Run
./build/src/cashew_node
```

### Linux (Arch/MSYS2)

```bash
# Install dependencies
sudo pacman -S base-devel cmake ninja libsodium spdlog \
  nlohmann-json blake3 gtest

# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON

# Build
cmake --build build

# Run
./build/src/cashew_node
```

## Running Tests

```bash
cd build
ctest --verbose
```

## Project Status

### ✅ Completed (Phase 1 - Foundation)
- [x] Project structure and CMake setup
- [x] Dependency management (vcpkg)
- [x] Cryptography wrappers (Ed25519, ChaCha20-Poly1305, BLAKE3)
- [x] Logging system (spdlog)
- [x] Configuration system (JSON)
- [x] Node identity system (key generation, storage, signing)
### Windows (MSYS2 UCRT64)

1. **Install MSYS2 and dependencies** (see above)

2. **Open MSYS2 UCRT64 terminal and build:**
   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

3. **Run the node:**
   ```bash
   ./build/src/cashew_node.exe
   ```

### Linux

1. **Install dependencies:**
   ```bash
   # Debian/Ubuntu
   sudo apt install libsodium-dev libspdlog-dev nlohmann-json3-dev libblake3-dev

   # Arch
   sudo pacman -S libsodium spdlog nlohmann-json blake3
   ```

2. **Build:**
   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

3. **Run the node:**
   ```bash
   ./build/src/cashew_node
   ```

On first run, the nodeencies via vcpkg:**
   ```bash
   vcpkg install libsodium spdlog nlohmann-json blake3 gtest
   ```

2. **Build the project:**
   ```bash
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
   cmake --build build
   ```

3. **Run the node:**
   ```bash
   ./build/src/cashew_node
   ```

   On first run, it will generate a new identity and save it to `cashew_identity.dat`.

## Configuration

Create a `cashew.conf` file (JSON format):

```json
{
  "log_level": "info",
  "log_to_file": false,
  "identity_file": "cashew_identity.dat",
  "identity_password": ""
}
```

## Development

- **Code style:** Follow existing C++20 style
- **Testing:** Write tests for new features
- **Documentation:** Update docs as needed

## License

MIT - See LICENSE file
