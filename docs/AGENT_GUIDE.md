# Agent Guide — Contributing to Gecko

This guide is for AI coding agents (Copilot, etc.) working on the Gecko engine.
Read this before making changes.

## Quick Reference

| What | Command |
|------|---------|
| Build (debug) | `gk build debug` |
| Build (release) | `gk build release` |
| Run tests | `gk test debug` |
| Run feature tests | `gk test debug --feature` |
| Run an example | `gk run platform_example debug` |
| Format code | `gk format` |
| Clean build | `gk clean` |

All commands require `source scripts/setup.sh` first.

## Compiler & Standard

- **GCC 15+** on both Linux and Windows (MinGW-w64 via MSYS2)
- **C++26** (`-std=c++2c`, shows as `gnu++26`)
- MSVC is not supported yet (no C++26 support as of mid-2026)
- Write MSVC-compatible code where possible for future migration:
  - Use `_WIN32` not `_MSC_VER` for Windows platform checks
  - Use `<share.h>` not `<corecrt_share.h>`
  - Use CMake `target_link_libraries` not `#pragma comment(lib, ...)`
  - Avoid GCC-only builtins without `#ifdef` fallbacks

## Coding Conventions

### Naming
- **PascalCase** for types, methods, namespaces: `EventBus`, `GetAllocator()`
- **m_** prefix for private members: `m_WindowCount`
- **UPPER_SNAKE** for macros and constants: `GECKO_INFO`
- **camelCase** for local variables: `windowHandle`

### Style Rules
- `::` prefix for all global namespace items: `::std::vector`, `::Display*`
- Never use `std::printf` — use `GECKO_INFO`/`GECKO_WARN`/`GECKO_ERROR`
  (exception: after `GECKO_SHUTDOWN` when logger is torn down)
- Never use output reference parameters — always return values.
  Use pointers only if null is meaningful.
- Field designators must follow declaration order (`-Wreorder-init-list` with `-Werror`)
- Compile with `-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter`

### Logging
```cpp
GECKO_INFO("gecko.module.category", "Message: {}", value);
GECKO_WARN("gecko.module.category", "Warning: {}", value);
GECKO_ERROR("gecko.module.category", "Error: {}", value);
```

## Architecture

### Module Structure
```
include/gecko/<module>/    # Public headers
src/<module>/              # Implementation
src/<module>/private/      # Internal headers (not public API)
test/<module>/             # Tests (Catch2 v3)
```

Modules: **Core**, **Platform**, **Runtime**, **Math**

### Dependency Rules
- Core depends on nothing
- Platform depends on Core
- Runtime depends on Core (may use Platform)
- Math is header-only, depends on nothing

### Service Pattern
Core defines `I*` interfaces + `Null*` defaults.
Runtime provides concrete implementations.
Apps install via `Services{...}`, access via `GetX()`.

```cpp
// Core: interface
class IAllocator { virtual void* Allocate(...) = 0; };

// Runtime: implementation
class TrackingAllocator : public IAllocator { ... };

// App: install
gecko::core::InstallServices({ .Allocator = &myAllocator });
```

## Platform Backends

Platform code is split by OS:
- `src/platform/win32/` — Win32 API (windows, monitors)
- `src/platform/linux/` — X11 + Wayland backends

Windows guards: `#if defined(GECKO_PLATFORM_WINDOWS)` or `#if defined(_WIN32)`
Linux guards: `#if defined(GECKO_PLATFORM_LINUX)`

## Build System

- CMake 3.22+, Ninja Multi-Config generator
- Build output: `out/<PlatformID>/bin/<Config>/`
- CMake internals: `out/build/<PlatformID>/`
- PlatformID examples: `Linux-x86_64`, `Windows-x86_64`

### Adding a New Module
```bash
gk new-module <name>    # Scaffolds include/src/CMakeLists.txt
```

### Adding a New Example
```bash
gk new-example <name>   # Scaffolds example app
```

## Testing

- Framework: Catch2 v3.5.1 (fetched via CMake FetchContent)
- Test binaries: `out/<PlatformID>/bin/Debug/tests/`
- Unit tests run headless — no display needed
- Feature tests (`--feature`) need a live display/window server

### Test File Location
```
test/<module>/test_<feature>.cpp
```

### Test Pattern
```cpp
#include <catch2/catch_test_macros.hpp>
#include <gecko/core/some_header.h>

TEST_CASE("Feature description", "[module][tag]") {
    // Arrange
    // Act
    // Assert
    REQUIRE(result == expected);
}
```

## Windows Development (MSYS2)

- All Windows builds use MSYS2 UCRT64 environment
- `platform.system()` returns `"MINGW64_NT-10.0-..."` not `"Windows"` in MSYS2 Python
- Use `_is_windows()` from `scripts/commands/__init__.py` for platform detection
- MinGW runtime DLLs are auto-copied to the build output directory
- Network shares (SMB/mapped drives) can cause issues:
  - SmartScreen blocks unsigned executables — test/run scripts copy to temp dirs
  - Ghost directories after failed deletes — close all programs accessing the share
  - `ctypes.windll` unavailable in MSYS2 Python — use `net use` fallback

## Things to Avoid

- Don't open windows or run the platform example unless explicitly asked
- Don't add docstrings/comments/type annotations to code you didn't change
- Don't add error handling for scenarios that can't happen
- Don't create abstractions for one-time operations
- Don't use `sleep` or blocking waits in scripts — check exit codes
- Don't modify `.vscode/` files without asking (they're gitignored and local)

## Branching & PRs

- Branch from `dev`, not `main`
- Branch naming: `feature/<name>`, `fix/<name>`, `chore/<name>`
- PRs target `dev` (GitHub defaults to `main` — change it)
- Atomic commits preferred — one logical change per commit
- Run `gk test debug` and `gk build debug` before committing

## Version

Current: `v0.0.0-alpha.2`

Versioning: `v0.0.0-{stage}.{N}` during development.
Alpha → Beta → Stable (`0.1.0`+).
