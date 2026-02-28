# Build Guide

## Requirements

- CMake 3.22+
- Python 3.7+
- Ninja
- **Clang 17+** (required - project does not support GCC or MSVC)
- Linux: `libx11-dev libxext-dev`
- Windows: Visual Studio Build Tools (for Windows SDK headers/libs)

### Installing Clang

**Windows:**
- Visual Studio Installer → Modify → Individual Components → "C++ Clang Compiler"
- Or standalone: `winget install LLVM.LLVM`

**Linux:**
```bash
sudo apt-get install clang  # Ubuntu/Debian
sudo dnf install clang      # Fedora
```

**macOS:**
```bash
brew install llvm
```

## Setup

```bash
# Source the dev environment (creates gk function)
source scripts/setup.sh    # Linux/macOS
. .\scripts\setup.ps1      # PowerShell
```

The setup script will:
1. Detect and configure Clang compiler
2. On Windows: Load Visual Studio environment and find clang-cl
3. Configure CMake with Ninja Multi-Config generator

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
├── build/           # CMake internals (hidden)
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
