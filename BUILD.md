# Building Cashew

This guide is the canonical build path for local development and CI parity.

## Quick Paths

Windows all-in-one (build + restart stack + optional firewall + health check):

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\bootstrap-windows.ps1
```

Manual setup helpers:

```bash
./setup-windows.sh
./setup-linux.sh
```

## Prerequisites

Windows (MSYS2 UCRT64):
- mingw-w64-ucrt-x86_64-gcc
- mingw-w64-ucrt-x86_64-cmake
- mingw-w64-ucrt-x86_64-ninja
- mingw-w64-ucrt-x86_64-libsodium
- mingw-w64-ucrt-x86_64-spdlog
- mingw-w64-ucrt-x86_64-nlohmann-json
- mingw-w64-ucrt-x86_64-openssl
- mingw-w64-ucrt-x86_64-gtest

Linux (Debian/Ubuntu):
- build-essential
- cmake
- ninja-build
- pkg-config
- libsodium-dev
- libspdlog-dev
- nlohmann-json3-dev
- libssl-dev
- libgtest-dev

Note: BLAKE3 is built from the bundled third_party/BLAKE3 submodule.

## Windows Build (MSYS2 UCRT64)

```bash
pacman -Syu
pacman -S --needed base-devel \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-libsodium \
  mingw-w64-ucrt-x86_64-spdlog \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-gtest

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Binary output:
- build/src/cashew.exe

## Windows Build (PowerShell)

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run:

```powershell
.\build\src\cashew.exe
```

If you launch from plain PowerShell and hit missing DLL errors, run stack startup through:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\start-stack.ps1
```

That startup script auto-adds detected MSYS2 UCRT runtime path for the current session.

## Linux Build

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  libsodium-dev libspdlog-dev nlohmann-json3-dev \
  libssl-dev libgtest-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Binary output:
- build/src/cashew

## Hosting Stack Commands

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\start-stack.ps1
powershell -ExecutionPolicy Bypass -File .\deploy\health-check.ps1
powershell -ExecutionPolicy Bypass -File .\deploy\stop-stack.ps1
```

Linux:

```bash
./deploy/start-stack.sh
./deploy/health-check.sh
./deploy/stop-stack.sh
```

## CI Parity

These commands mirror CI behavior:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

If system GTest is unavailable, CMake fetches googletest automatically during configure.

## Troubleshooting

1. Configure fails on GTest:
- Install gtest package for your platform, or rely on automatic FetchContent fallback.

2. Windows EXE fails with missing DLL:
- Ensure MSYS2 UCRT64 runtime is installed.
- Start with deploy/start-stack.ps1 (it prepends runtime path automatically).

3. Port in use errors on startup:
- Run deploy/stop-stack.ps1, then deploy/start-stack.ps1.
