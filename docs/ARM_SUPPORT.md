# ARM Architecture Support

## Overview

Cashew Framework is fully compatible with ARM processors, including:
- **ARM 64-bit (AArch64/ARM64)**: Raspberry Pi 3/4/5 (64-bit OS), Apple M1/M2, AWS Graviton
- **ARM 32-bit (ARMv7/ARMv8)**: Raspberry Pi 2/3/4 (32-bit OS), older embedded systems

## Features

### Architecture-Specific Optimizations

The build system automatically detects your CPU architecture and applies appropriate optimizations:

#### ARM 64-bit (aarch64/arm64)
- Uses `-mcpu=native` for optimal CPU-specific tuning
- Enables ARM NEON SIMD instructions
- BLAKE3 uses ARM crypto extensions when available

#### ARM 32-bit (armv7/armv7l)
- Uses `-mcpu=native` for optimal CPU-specific tuning
- Explicitly enables NEON SIMD with `-mfpu=neon`
- Optimized for Raspberry Pi 2/3/4 running 32-bit OS

#### x86/x64 (Intel/AMD)
- Uses `-march=native` for optimal CPU-specific tuning
- Enables SSE2/SSE4.1/AVX2/AVX512 when available

### Cross-Compilation Support

The build system respects `CMAKE_CROSSCOMPILING` flag and skips native CPU optimizations when cross-compiling, ensuring compatibility with target platforms.

## Building for Raspberry Pi

### Quick Start

```bash
# Clone the repository
git clone <repository-url> cashew
cd cashew

# Run the Raspberry Pi setup script
chmod +x setup-raspberry-pi.sh
./setup-raspberry-pi.sh
```

### Manual Build

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build \
    libsodium-dev nlohmann-json3-dev pkg-config git

# Install spdlog (if not available in your distro)
# See setup-raspberry-pi.sh for details

# Build
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)

# Test
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

## Performance Considerations

### Raspberry Pi Performance Tips

1. **Use 64-bit OS when possible**
   - Raspberry Pi 3/4/5 support 64-bit
   - Better performance and larger memory addressing
   - `uname -m` should show `aarch64`

2. **Cooling**
   - Crypto operations are CPU-intensive
   - Use heatsinks or active cooling for sustained loads
   - Monitor temperature: `vcgencmd measure_temp`

3. **Memory**
   - Minimum 2GB RAM recommended for node operation
   - 4GB+ recommended for development/testing
   - Consider swap if building with limited RAM

4. **Storage**
   - Use fast SD card (Class 10/U3/A2 minimum)
   - Or USB 3.0 SSD for better I/O performance
   - Ledger data can grow over time

### Expected Performance

On Raspberry Pi 4 (4GB, Quad-core Cortex-A72):
- Build time: ~5-10 minutes
- BLAKE3 hash: ~100-200 MB/s (with NEON)
- Ed25519 signing: ~5000-10000 ops/sec
- Network throughput: Limited by 1 Gbps Ethernet

On Raspberry Pi 5 (8GB, Quad-core Cortex-A76):
- Build time: ~3-5 minutes
- BLAKE3 hash: ~200-400 MB/s (with crypto extensions)
- Ed25519 signing: ~10000-20000 ops/sec
- Network throughput: Limited by PCIe bandwidth

## Architecture Detection

The build system logs architecture information:

```
-- Target architecture: aarch64
-- Building for Linux
-- Cross-compiling: FALSE
```

Check your current architecture:
```bash
uname -m  # aarch64, armv7l, x86_64, etc.
```

## Dependencies on ARM

All Cashew dependencies support ARM:

| Dependency | ARM Support | Notes |
|------------|-------------|-------|
| libsodium | Full | Native ARM crypto optimizations |
| BLAKE3 | Full | ARM NEON/crypto extensions |
| spdlog | Full | Header-only, architecture-agnostic |
| nlohmann-json | Full | Header-only, architecture-agnostic |
| CMake | Full | Native ARM packages available |

## Cross-Compilation

### Building for ARM on x86/x64

```bash
# Install cross-compilation toolchain
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Create toolchain file (arm64-toolchain.cmake)
cat > arm64-toolchain.cmake << 'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

# Build
mkdir build-arm64 && cd build-arm64
cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=../arm64-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    ..
cmake --build .
```

**Note**: Cross-compiled binaries skip `-mcpu=native` optimizations for portability.

## Testing on ARM

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run specific tests
./tests/test_crypto
./tests/test_network
./tests/test_ledger_reputation

# Benchmark cryptographic operations
./tests/test_crypto --benchmark
```

## Troubleshooting

### Build Fails with "illegal instruction"
- You may be running binaries built with native optimizations on different CPU
- Rebuild with `-DCMAKE_BUILD_TYPE=Debug` or on the target device

### Slow Compilation
- Use `-j1` instead of `-j$(nproc)` to reduce memory usage
- Add swap space if running out of memory
- Consider cross-compiling from a more powerful machine

### Missing Dependencies
- Raspberry Pi OS Lite may lack some packages
- Install full development tools: `sudo apt-get install build-essential`
- Some dependencies may need manual installation (see setup script)

### BLAKE3 Performance Lower Than Expected
- Ensure you're using 64-bit OS on compatible hardware
- Check that NEON is enabled: `cat /proc/cpuinfo | grep neon`
- Verify build type is Release: `cmake -LA build | grep CMAKE_BUILD_TYPE`

## Platform-Specific Notes

### Raspberry Pi OS (32-bit)
- Maximum 4GB RAM addressable per process
- NEON SIMD explicitly enabled with `-mfpu=neon`
- Consider upgrading to 64-bit OS for better performance

### Raspberry Pi OS (64-bit)
- Full memory addressing on 4GB/8GB models
- Better compiler optimizations available
- ARM crypto extensions may be available (Pi 4/5)

### Ubuntu on Raspberry Pi
- Usually 64-bit by default
- Newer package versions available
- May have better ARM optimization support

## Contributing

When contributing ARM-specific improvements:
1. Test on both 32-bit and 64-bit ARM
2. Verify cross-compilation still works
3. Update this documentation
4. Add architecture info to commit messages

## References

- [ARM NEON Intrinsics](https://developer.arm.com/architectures/instruction-sets/intrinsics/)
- [BLAKE3 ARM Implementation](https://github.com/BLAKE3-team/BLAKE3)
- [Raspberry Pi Documentation](https://www.raspberrypi.com/documentation/)
- [libsodium ARM Optimizations](https://doc.libsodium.org/)
