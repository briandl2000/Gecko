# Gecko Engine

A modular C++26 game/application engine focused on clean architecture and fast iteration.

## Prerequisites

Install these **before** cloning:

| Tool | Min version | Notes |
|------|-------------|-------|
| **GCC** | 15 | C++26 (`-std=c++2c`). Windows: via MSYS2 UCRT64. MSVC not yet supported. |
| **CMake** | 3.22 | |
| **Ninja** | any | Multi-config generator |
| **Python** | 3.7 | For the `gk` CLI wrapper |
| **Vulkan SDK** | 1.3 | Required for the Vulkan graphics backend (see below). |

Linux also needs the X11 / Wayland dev packages. One-liners per distro and the
Vulkan install commands live in [docs/build.md](docs/build.md#prerequisites).

> **Vulkan SDK is optional only if you don't need rendering.** Without it
> `CreateGraphicsDevice()` falls back to the `NullDevice` and every draw call
> is a no-op. When missing, CMake prints the exact command to install it.

## Quick Start

```bash
git clone https://github.com/yourusername/gecko.git
cd gecko

source scripts/setup.sh    # Linux / macOS / MSYS2 — creates `gk` shell function
gk build                   # Debug build
gk test                    # Run unit tests
gk run graphics_example    # Run the graphics demo
```

## CLI Commands

| Command | Description |
|---------|-------------|
| `gk setup [--clean]` | Configure CMake |
| `gk build [debug\|release\|all]` | Build the engine libraries |
| `gk build examples [debug\|release]` | Build example applications |
| `gk test [debug\|release]` | Build and run tests |
| `gk run <example> [debug\|release]` | Build and run an example |
| `gk format` | `clang-format` all sources |
| `gk package [local\|dev\|release]` | Create a distributable package |

## Project Structure

```
gecko/
├── include/gecko/    # Public headers
│   ├── core/         # Core utilities, services, logging
│   ├── graphics/     # Graphics module (Vulkan 1.3 + NullDevice)
│   ├── math/         # Math library (vectors, matrices, rotors)
│   ├── platform/     # Platform abstraction (windows, input, monitors)
│   └── runtime/      # Runtime services (event bus, profiling, job system)
├── src/              # Implementation
├── test/             # Unit tests (Catch2 v3)
├── examples/         # Example applications
├── scripts/          # Build scripts and `gk` CLI
└── out/              # All build outputs (gitignored)
```

## Versioning

`v{major}.{minor}.{patch}` for stable releases; `v0.0.0-{stage}.{N}` while
pre-alpha. Stages: **alpha** → **beta** → **stable** (`0.1.0`+).

## License

MIT
