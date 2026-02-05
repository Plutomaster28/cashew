# Building Cashew on Windows with MSYS2/UCRT64 🥜

## Why MSYS2/UCRT64?

MSYS2 provides a native Unix-like environment on Windows with:
- **Modern GCC toolchain** (GCC 13+)
- **pacman package manager** - easy dependency installation
- **UCRT64** - Universal C Runtime, modern and actively maintained
- **Native performance** - no WSL overhead
- **Easy integration** with Windows tools

## Installation Guide

### Step 1: Install MSYS2

1. Download from: https://www.msys2.org/
2. Run the installer (e.g., `msys2-x86_64-20240113.exe`)
3. Install to `C:\msys64` (default)
4. Check "Run MSYS2 now" and finish

### Step 2: Update MSYS2

Open **MSYS2 UCRT64** (the purple icon) and run:

```bash
pacman -Syu
```

If it asks to close the terminal, close it and reopen MSYS2 UCRT64, then run:

```bash
pacman -Su
```

### Step 3: Install Build Tools

Install the complete toolchain:

```bash
pacman -S --needed base-devel \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja
```

4. **Install Cashew Dependencies

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-libsodium \
  mingw-w64-ucrt-x86_64-spdlog \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-gtest
```

**Note:** BLAKE3 might not be available in MSYS2 repos. If `mingw-w64-ucrt-x86_64-blake3` is not found, you can either:
- Use the bundled implementation (automatic)
- Build it from source: `./build-blake3.sh`

### Step 5: Verify Installation

```bash
gcc --version      # Should show gcc 13.x or newer
cmake --version    # Should show 3.20+
ninja --version    # Should show 1.11+
```

## Building Cashew

### From MSYS2 UCRT64 Terminal

```bash
# Navigate to your project (Windows paths use /c/ instead of C:\)
cd /c/Users/YourName/OneDrive/Documents/cashew

# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASHEW_BUILD_TESTS=ON

# Build
cmake --build build

# Run
./build/src/cashew_node.exe

# Run tests
cd build
ctest --verbose
```

### From Windows Terminal/PowerShell (Optional)

If you want to build from PowerShell/CMD:

1. **Add UCRT64 to PATH:**
   - Open Windows Settings → System → About → Advanced system settings
   - Environment Variables → System Variables → Path → Edit
   - Add: `C:\msys64\ucrt64\bin`
   - Move it above any other MinGW/MSYS entries

2. **Build from PowerShell:**
   ```powershell
   cd C:\Users\YourName\OneDrive\Documents\cashew
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   .\build\src\cashew_node.exe
   ```

## Common Issues & Solutions

### "CMake cannot find compiler"

**Solution:** Make sure you're using **MSYS2 UCRT64** terminal (purple icon), not MSYS2 MSYS or MinGW64.

### "Cannot find libsodium/spdlog/etc."

**Solution:** Install dependencies in UCRT64 environment:
```bash
pacman -S mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-spdlog
```

### "DLL not found" when running

**Solution:** Either:
- Run from MSYS2 UCRT64 terminal (paths are set correctly), or
- Add `C:\msys64\ucrt64\bin` to your Windows PATH

### Slow compilation

**Solution:** Use Ninja (faster than Make) and enable parallel builds:
```bash
cmake --build build -j $(nproc)
```

## IDE Integration

### VS Code

1. Install "C/C++" extension
2. Add to `.vscode/settings.json`:
   ```json
   {
     "cmake.cmakePath": "C:/msys64/ucrt64/bin/cmake.exe",
     "cmake.generator": "Ninja",
     "cmake.configureEnvironment": {
       "PATH": "C:/msys64/ucrt64/bin;${env:PATH}"
     }
   }
   ```

### CLion

1. Settings → Build, Execution, Deployment → Toolchains
2. Add new toolchain:
   - **Name:** MSYS2 UCRT64
   - **CMake:** `C:\msys64\ucrt64\bin\cmake.exe`
   - **C Compiler:** `C:\msys64\ucrt64\bin\gcc.exe`
   - **C++ Compiler:** `C:\msys64\ucrt64\bin\g++.exe`
   - **Debugger:** `C:\msys64\ucrt64\bin\gdb.exe`

## Development Workflow

```bash
# Daily workflow
cd /c/Users/YourName/OneDrive/Documents/cashew

# Make changes to code...

# Quick rebuild (only changed files)
cmake --build build

# Full rebuild
cmake --build build --clean-first

# Run tests
cd build && ctest --output-on-failure

# Debug build (with symbols)
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
gdb ./build-debug/src/cashew_node.exe
```

## Performance Notes

- **UCRT64 vs MinGW64:** UCRT64 uses the modern Universal C Runtime, better Windows integration
- **Ninja vs Make:** Ninja is significantly faster for incremental builds
- **Release builds:** Always use `-DCMAKE_BUILD_TYPE=Release` for performance testing
- **LTO:** Add `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` for extra optimization

## Updating Dependencies

```bash
# Update all packages
pacman -Syu

# Update specific package
pacman -S mingw-w64-ucrt-x86_64-libsodium

# Search for packages
pacman -Ss libsodium
```

## Need Help?

- MSYS2 documentation: https://www.msys2.org/docs/
- MSYS2 packages: https://packages.msys2.org/
- Cashew project docs: See BUILD.md and README.md

---

**🥜 Happy building!**
