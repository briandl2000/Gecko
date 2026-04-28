# Build Guide

## Prerequisites

Install everything in this section **before** running `gk build`. The setup
script checks for these; missing pieces produce loud errors with the exact
install command for your platform.

### Toolchain

| Tool | Min version |
|------|-------------|
| C++ compiler | any with C++23 support |
| CMake | 3.22 |
| Ninja | any |
| Python | 3.7 |

Gecko targets **C++23** and lets CMake auto-detect the host compiler.
Tier-1 verified configurations:

- GCC 13+ on Linux (x86_64).
- aarch64-linux-gnu-g++ via the docker toolchain in `docker/aarch64-toolchain.cmake` (used by `gk-pi`).
- MSVC (Visual Studio 2022 17.6+) on Windows.

Clang on Linux should also work but is not gated in CI.

#### Linux (Ubuntu / Debian)
```bash
sudo apt-get install gcc g++ ninja-build cmake python3 \
  libx11-dev libxext-dev libxrandr-dev libwayland-dev wayland-protocols
```

#### Linux (Fedora)
```bash
sudo dnf install gcc g++ ninja-build cmake python3 \
  libX11-devel libXext-devel libXrandr-devel wayland-devel wayland-protocols-devel
```

#### Linux (Arch)
```bash
sudo pacman -S gcc ninja cmake python libx11 libxext libxrandr wayland wayland-protocols
```

#### Windows

1. Install Visual Studio 2022 17.6+ with the "Desktop development with C++" workload.
2. Install CMake, Ninja, and Python 3 (the VS installer can include CMake / Ninja).
3. Run `gk` from a *Developer PowerShell for VS 2022* so `cl.exe` is on PATH.

Gecko's Windows setup script (`scripts/setup.ps1`) is a thin convenience
wrapper; you can also drive CMake manually with the presets in
`CMakePresets.json`. MinGW-w64 also works if you want a Linux-style
workflow, but MSVC is the supported default.

#### macOS
```bash
brew install gcc ninja cmake python
```

### Vulkan SDK

The Graphics module's real backend is Vulkan 1.3 (dynamic rendering + sync2).
Without the SDK `CreateGraphicsDevice()` falls back to a `NullDevice` (all
calls are no-ops) — useful for CI/headless builds, useless for rendering.

Gecko uses the standard CMake `find_package(Vulkan)`, which picks up both
system packages and the LunarG SDK (via `$VULKAN_SDK`). When the SDK is
missing, CMake prints the exact install command for your platform.

| Platform | Install |
|----------|---------|
| Ubuntu / Debian | `sudo apt-get install libvulkan-dev vulkan-validationlayers spirv-tools` |
| Fedora | `sudo dnf install vulkan-loader-devel vulkan-validation-layers spirv-tools` |
| Arch | `sudo pacman -S vulkan-devel spirv-tools` |
| Windows | [LunarG Vulkan SDK](https://vulkan.lunarg.com/) (sets `$VULKAN_SDK`) |
| macOS | [LunarG SDK for macOS](https://vulkan.lunarg.com/) (bundles MoltenVK) |

**Shader compiler:** `glslc` ships with every SDK above. Examples that load
shaders (e.g. `graphics_example`) compile HLSL → SPIR-V at build time via
`glslc`. A missing `glslc` disables shader compilation for that example.

**Validation layers:** Debug builds enable `VK_LAYER_KHRONOS_validation` when
it is installed. Shipping builds don't require it.

## Setup

**Linux / macOS / MSYS2 (Bash):**

```bash
source scripts/setup.sh
```

**Windows (Developer PowerShell for VS 2022):**

```powershell
. .\scripts\setup.ps1
```

The setup script defines the `gk` command and runs the initial CMake
configure. CMake auto-detects the host C++ compiler; set `CC` / `CXX`
(or `CMAKE_TOOLCHAIN_FILE`) before sourcing the script if you want to
pin a specific one. Re-run any time `CMakePresets.json` or prerequisites
change.

## `gk` CLI

```bash
gk build                 # Debug build
gk build release         # Release build
gk build all             # Both
gk test                  # Build + run unit tests (Debug)
gk test --build-only     # Build tests without running
gk run <example>         # Build + run an example
gk format                # clang-format all sources
gk clean                 # Wipe out/ and reconfigure
gk package [local|dev|release]
```

## Manual CMake

```bash
cmake --preset debug
cmake --build out/build --config Debug
cmake --install out/build --prefix /your/prefix --config Release
```

## Output Structure

```
out/
├── build/<PlatformID>/   # CMake internals
└── <PlatformID>/
    ├── bin/{Debug,Release}/
    ├── lib/{Debug,Release}/
    └── package/
```

`<PlatformID>` is `<OS>-<arch>`, e.g. `Linux-x86_64`, `Windows-x86_64`.

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `GECKO_BUILD_EXAMPLES` | ON | Build example applications |
| `GECKO_BUILD_TESTS` | OFF | Build unit tests (auto-enabled by `gk test`) |
| `GECKO_OVERRIDE_NEW` | OFF | Route global `new`/`delete` through Gecko's allocator |

## VS Code Debugging

Pre-configured launch targets in `.vscode/launch.json`:

- **Local: Debug example** — GDB on Linux (pick Debug/Release + target)

Set breakpoints, press `F5`, pick a target. The Local target builds via
`gk: build` before launching.

## MinGW Runtime DLLs

When building with MinGW-w64 the build copies `libgcc_s_seh-1.dll`,
`libstdc++-6.dll`, and `libwinpthread-1.dll` alongside each executable
so nothing depends on the MinGW install being on `PATH`. With MSVC and
on Linux this step is a no-op.

## ABI Notes

`CoreServices` is built as a shared library so the engine and (future)
plugins observe a single set of service singletons. Virtual methods on
`CoreServices` interfaces (`IAllocator`, `IJobSystem`, `IProfiler`,
`ILogger`, `IEventBus`, `IModule`, `IProfilerSink`, ...) only use
layout-stable types -- primitives, Gecko PODs, raw pointers, and
`gecko::Span<T>` (a fixed `{ T*, size_t }` POD in
[`include/gecko/core/span.h`](../include/gecko/core/span.h)) -- so the
boundary survives differences in standard-library version between the
engine binary and a plugin.

Free functions and class methods on the static `Core`, `Platform`,
`Math`, and `Runtime` libraries may use `std::span`, `std::string`,
`std::string_view`, etc. freely; each linking binary gets its own copy
of those static libraries.

## VS Code Tasks & Debugging

`.vscode/tasks.json` provides `gk: build debug`, `gk: build release`,
`gk: test debug`, `gk: test release`, and `gk: format`. Default build
task is `gk: build debug` (`Ctrl+Shift+B`).

`.vscode/launch.json` defines flat per-OS-per-config launch entries.
Pick the one for your OS + configuration; the target (example or test
binary) is chosen via a single `debugTarget` picker on launch.

| Launch config | What it runs |
|---|---|
| **Linux Debug** / **Linux Release** | Native `gdb` against `out/Linux-x86_64/bin/{Debug,Release}/`. Uses `LD_LIBRARY_PATH` so the app finds `libCoreServices.so`. Prelaunch: `gk: build {debug,release}`. |
| **Windows Debug** / **Windows Release** | MSYS2 UCRT64 `gdb.exe` against `C:\Gecko\{debug,release}\`. Prelaunch builds on the Windows machine and mirrors artefacts there — see below. |

### Windows network-drive workaround

A common Gecko setup is: the workspace lives on a Linux or NAS machine,
and a Windows machine on the same LAN mounts it as a network drive
(e.g. `Z:\`). SmartScreen refuses to launch unsigned `.exe` files from
network drives. To work around this, the Windows launch configs build
into `Z:\out\Windows-x86_64\bin\{Debug,Release}\` as usual, then
mirror the artefacts to a plain local path — `C:\Gecko\debug\` or
`C:\Gecko\release\` — before debugging. Handled by
[`scripts/copy_debug.py`](../scripts/copy_debug.py), which is a no-op
on Linux/macOS so the same `.vscode/` works everywhere.

