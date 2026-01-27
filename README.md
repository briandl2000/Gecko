# Gecko Engine

A modular C++23 game/application engine focused on clean architecture and fast iteration.

## Quick Start

```bash
# Clone
git clone https://github.com/yourusername/gecko.git
cd gecko

# Setup dev environment (creates gk function for this shell)
source scripts/setup.sh    # Linux/macOS
# or
. .\scripts\setup.ps1      # PowerShell

# Build
gk build

# Run tests
gk test
```

## Requirements

- **CMake** 3.22+
- **Python** 3.7+
- **Ninja** (recommended)
- **GCC 13+** or **Clang 17+** with C++23 support

## CLI Commands

| Command | Description |
|---------|-------------|
| `gk setup [--clean]` | Configure CMake |
| `gk build [debug\|release\|all]` | Build the engine |
| `gk test [debug\|release]` | Build and run tests |
| `gk format` | Format all source files |
| `gk package [local\|dev\|release]` | Create distributable package |

## Project Structure

```
gecko/
├── include/gecko/    # Public headers
│   ├── core/         # Core utilities, services, logging
│   ├── math/         # Math library (vectors, matrices, rotors)
│   ├── platform/     # Platform abstraction (window, input)
│   └── runtime/      # Runtime services (event bus, profiling)
├── src/              # Implementation files
├── test/             # Unit tests (Catch2)
├── examples/         # Example applications
├── scripts/          # Build scripts and CLI
└── out/              # All build outputs (gitignored)
```

## Versioning

Format: `v{major}.{minor}.{patch}[-{prerelease}]`

- **Prerelease**: `alpha.N`, `beta.N`, or empty for stable
- **Dev builds**: Append `-dev.{timestamp}` (automatic in CI)

## CI/CD

- **Pull requests**: Build and test on Linux & Windows
- **Push to dev**: Build, test, create dev release artifact
- **Push to main**: Build, test, create tagged GitHub release

## License

MIT
