# Build Guide

## Requirements

- CMake 3.22+
- Python 3.7+
- Ninja
- **GCC 14+** (C++26 support required)
- Linux: `libx11-dev libxext-dev libxrandr-dev libwayland-dev wayland-protocols`

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
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-cmake
```
4. Add `C:\msys64\ucrt64\bin` to your system PATH
5. Verify: `gcc --version` should show GCC 14+

> **Note:** MSVC does not support C++26. Windows builds use MinGW-w64 GCC via MSYS2.

**macOS:**
```bash
brew install gcc ninja cmake
```

## Setup

```bash
# Source the dev environment (creates gk function)
source scripts/setup.sh    # Linux/macOS
. .\scripts\setup.ps1      # PowerShell
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

- **Debug: app_skeleton** - Minimal application skeleton
- **Debug: core_example** - Core services demo
- **Debug: math_example** - Math library demo  
- **Debug: platform_example** - Platform/window demo

To debug:
1. Set breakpoints in your code
2. Press `F5` or Run → Start Debugging
3. Select the example you want to debug

All configurations automatically build before launching.

## VS Code Tasks

Pre-configured tasks in `.vscode/tasks.json`:

- **gk build** (default) - Build Debug configuration
- **gk test** - Build and run tests
- **gk clean** - Clean and reconfigure CMake

Run with: `Ctrl+Shift+B` (default build) or `Ctrl+Shift+P` → `Tasks: Run Task`
