# Build Fixes for UCRT64 Compatibility

## Date: 2026-01-31

### Issues Fixed

#### 1. **CMake Linker Language Error**
**Problem:** CMake could not determine linker language for `blake3_bundled` target.

**Solution:**
- Added `C` to project languages in root CMakeLists.txt: `project(Cashew VERSION 0.1.0 LANGUAGES C CXX)`
- Set `LINKER_LANGUAGE C` property on blake3_bundled target
- Added `blake3.h` to sources list in blake3 CMakeLists.txt

**Files Modified:**
- `CMakeLists.txt` (root)
- `third_party/blake3/CMakeLists.txt`

#### 2. **CASHEW_PLATFORM_WINDOWS Redefinition**
**Problem:** CMake was passing `-DCASHEW_PLATFORM_WINDOWS` as a compile definition, but `common.hpp` was also defining it unconditionally, causing a redefinition error with `-Werror`.

**Solution:**
- Changed platform detection to only define if not already defined:
  ```cpp
  #ifndef CASHEW_PLATFORM_WINDOWS
      #define CASHEW_PLATFORM_WINDOWS
  #endif
  ```

**Files Modified:**
- `include/cashew/common.hpp`

#### 3. **Missing Standard Library Headers**
**Problem:** `common.hpp` was using `uint16_t`, `uint32_t`, `uint8_t`, and `size_t` without including required headers, causing "does not name a type" errors.

**Solution:**
- Moved `#include <cstdint>` and `#include <cstddef>` to the top of the file (after `#pragma once`)
- Moved all standard library includes before any code

**Files Modified:**
- `include/cashew/common.hpp`

#### 4. **Constructor Overload Conflict**
**Problem:** `NodeID` had two constructors that both took `fixed_bytes<32>`:
- `explicit NodeID(const Hash256& hash)`
- `explicit NodeID(const PublicKey& pubkey)`

Since both `Hash256` and `PublicKey` are type aliases for `fixed_bytes<32>`, the compiler couldn't distinguish them.

**Solution:**
- Removed the `PublicKey` constructor declaration from the header
- Removed the corresponding implementation from `common.cpp`
- Users can now create NodeID from either Hash256 or PublicKey (they're the same type)

**Files Modified:**
- `include/cashew/common.hpp`
- `src/common.cpp`

#### 5. **Invalid Include Path**
**Problem:** `common.cpp` was using `#include "../crypto/blake3.hpp"` but the include path should be relative to the source directory.

**Solution:**
- Changed to `#include "crypto/blake3.hpp"` (CMake configures src/ as an include directory)

**Files Modified:**
- `src/common.cpp`

#### 6. **Missing Includes in node.cpp**
**Problem:** `node.cpp` was using `<thread>` and `<chrono>` without including them.

**Solution:**
- Added `#include <thread>` and `#include <chrono>` to node.cpp

**Files Modified:**
- `src/core/node/node.cpp`

### Build Result

**Successful Build**
- All 17 targets compiled successfully
- Blake3 bundled library linked correctly
- cashew_core library built
- cashew_node executable built
- test_crypto executable built
- test_identity executable built

### Testing

The build was verified by:
1. Running a clean CMake configuration
2. Building with ninja in verbose mode
3. Running `cashew_node.exe --help` successfully

### Key Takeaways

1. **Include Guards vs Preprocessor Definitions**: When CMake passes preprocessor definitions, headers should check if they're already defined before defining them
2. **Type Aliases Don't Create New Types**: `using PublicKey = fixed_bytes<32>` and `using Hash256 = fixed_bytes<32>` create identical types to the compiler
3. **Include Order Matters**: Standard library headers with primitive types must be included before using those types
4. **CMake Include Paths**: When a directory is added to include paths, use paths relative to that directory
5. **Multi-Language Projects**: CMake needs explicit language declarations for projects mixing C and C++

### Build Environment

- **Toolchain:** MSYS2 UCRT64
- **Compiler:** GCC 15.2.0
- **Build System:** CMake 3.31.1 + Ninja 1.12.1
- **Platform:** Windows (UCRT64)
