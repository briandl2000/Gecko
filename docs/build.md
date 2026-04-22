# Build Guide

## Requirements

- CMake 3.22+
- Python 3.7+
- Ninja
- **GCC 15+** (C++26 support required)
- Linux: `libx11-dev libxext-dev libxrandr-dev libwayland-dev wayland-protocols`
- **Vulkan SDK (optional, recommended):** needed to build the Vulkan graphics
  backend. Without it, `CreateGraphicsDevice()` falls back to the `NullDevice`
  and anything that renders will be a no-op. See
  [Installing the Vulkan SDK](#installing-the-vulkan-sdk) below.

> **Why GCC?** Gecko targets C++26 (`-std=c++2c`) for early access to features
> like reflection. As of mid-2026, GCC is the only compiler with broad C++26
> support in a stable release. When MSVC and Clang ship C++26 support, we plan
> to re-evaluate and may add them as supported compilers.

### Installing GCC

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install gcc g++ ninja-build cmake
# Platform dependencies
sudo apt-get install libx11-dev libxext-dev libxrandr-dev libwayland-dev wayland-protocols
```

**Linux (Fedora):**
```bash
sudo dnf install gcc g++ ninja-build cmake
sudo dnf install libX11-devel libXext-devel libXrandr-devel wayland-devel wayland-protocols-devel
```

**Windows (MSYS2 + MinGW-w64):**
1. Install [MSYS2](https://www.msys2.org/) (follow the installer)
2. Open the **UCRT64** terminal (not MSYS or MINGW64)
3. Install the toolchain:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-python
```
4. All commands (`gk build`, `gk test`, etc.) must be run from the MSYS2 UCRT64 terminal
5. Verify: `gcc --version` should show GCC 15+

> **Note:** MSVC does not yet support C++26 (still completing C++23 as of mid-2026).
> Windows builds use MinGW-w64 GCC via MSYS2. See [MSVC Migration Plan](#msvc-migration-plan) below.

**macOS:**
```bash
brew install gcc ninja cmake
```

## Installing the Vulkan SDK

Gecko's graphics module uses Vulkan 1.3 with dynamic rendering. The SDK is
*optional* — without it, `GraphicsBackend::Vulkan` is unavailable and
`CreateGraphicsDevice()` returns a `NullDevice` (all calls are no-ops). Install
it to run anything that actually draws.

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libvulkan-dev vulkan-validationlayers vulkan-tools spirv-tools
```

**Linux (Fedora):**
```bash
sudo dnf install vulkan-loader-devel vulkan-validation-layers vulkan-tools spirv-tools
```

**Linux (Arch):**
```bash
sudo pacman -S vulkan-devel vulkan-validation-layers vulkan-tools spirv-tools
```

**Windows (MSYS2 UCRT64):**
```bash
pacman -S mingw-w64-ucrt-x86_64-vulkan-headers mingw-w64-ucrt-x86_64-vulkan-loader mingw-w64-ucrt-x86_64-vulkan-validation-layers mingw-w64-ucrt-x86_64-spirv-tools
```
Or install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/) system-wide.

**macOS:** Install the [LunarG SDK for macOS](https://vulkan.lunarg.com/),
which includes MoltenVK.

**Shader tooling:** Examples with shaders (e.g. `graphics_example`) require
`glslangValidator` (shipped with the Vulkan SDK / `spirv-tools` on most distros)
to compile GLSL → SPIR-V at build time.

**Validation layers:** Debug builds enable `VK_LAYER_KHRONOS_validation` when
it is present on the system. If the layer package is missing, the instance is
created without it — shipping builds do not require it.

## Setup

```bash
# Source the dev environment (creates gk function)
source scripts/setup.sh    # Linux/macOS/MSYS2
```

The setup script will:
1. Detect and configure GCC compiler
2. Configure CMake with Ninja Multi-Config generator

## CLI Commands

### Build
```bash
gk build              # Build Debug
gk build release      # Build Release
gk build all          # Build both
```

### Test
```bash
gk test               # Build and run tests (Debug)
gk test release       # Release tests
gk test --build-only  # Build without running
```

### Package
```bash
gk package            # Local package (gecko-0.0.0-local.tar.gz)
gk package dev        # Dev package with timestamp
gk package release    # Release package
```

### Other
```bash
gk setup              # Configure CMake
gk setup --clean      # Clean and reconfigure
gk format             # Format all source files
```

## Manual CMake

```bash
cmake --preset debug
cmake --build out/build --config Debug
cmake --build out/build --config Release
cmake --install out/build --prefix /your/prefix --config Release
```

## Output Structure

```
out/
├── build/<PlatformID>/  # CMake internals (hidden)
└── <PlatformID>/
    ├── bin/Debug/       # Debug binaries
    ├── bin/Release/     # Release binaries
    ├── lib/Debug/       # Debug libraries
    ├── lib/Release/     # Release libraries
    └── package/         # Created packages
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `GECKO_BUILD_EXAMPLES` | ON | Build example applications |
| `GECKO_BUILD_TESTS` | OFF | Build unit tests (auto-enabled by `gk test`) |

## VS Code Debugging

Pre-configured debug launch targets are available in `.vscode/launch.json`:

- **Debug: Target (Linux)** — GDB on Linux
- **Debug: Target (Windows)** — GDB via MSYS2 on Windows
- **Remote Pi: Target** — Remote GDB debugging on Raspberry Pi

To debug:
1. Set breakpoints in your code
2. Press `F5` or Run → Start Debugging
3. Select the target you want to debug

**Windows:** The debugger uses GDB from MSYS2 (`C:\msys64\ucrt64\bin\gdb.exe`).
The pre-launch task copies build artifacts to `C:\Gecko\debug\` to avoid
network share and SmartScreen issues.

**Linux:** Uses the system GDB (`/usr/bin/gdb`). Executables run directly from
the build output directory.

All configurations automatically build before launching.

## MinGW Runtime DLLs

On Windows, the build automatically copies MinGW runtime DLLs (`libgcc_s_seh-1.dll`,
`libstdc++-6.dll`, `libwinpthread-1.dll`) alongside the built executables.
This ensures programs work without requiring MSYS2's bin directory on PATH.

## MSVC Migration Plan

Gecko currently uses GCC exclusively because MSVC has no C++26 core language
support (still completing C++23 as of mid-2026). The plan for MSVC adoption:

1. **Now:** GCC 15 on both Linux and Windows (via MinGW-w64). Write
   MSVC-compatible code where possible — avoid GCC-only extensions.
2. **When MSVC ships C++26:** Add MSVC as a Windows compiler option alongside
   MinGW. Target: Windows builds with MSVC, Linux builds with GCC.
3. **Code guidelines for MSVC readiness:**
   - Use `_WIN32` not `_MSC_VER` for Windows platform checks
   - Use `<share.h>` not `<corecrt_share.h>` (both compilers support it)
   - Keep Windows API calls in `win32/` backend files
   - Use CMake `target_link_libraries` instead of `#pragma comment(lib, ...)`
   - Avoid GCC builtins — use standard C++ or `#ifdef` with MSVC equivalents

## VS Code Tasks

Pre-configured tasks in `.vscode/tasks.json`:

- **gk build** (default) - Build Debug configuration
- **gk test** - Build and run tests
- **gk clean** - Clean and reconfigure CMake

Run with: `Ctrl+Shift+B` (default build) or `Ctrl+Shift+P` → `Tasks: Run Task`
