# Build Guide

## Requirements

- CMake 3.22+
- Python 3.7+
- Ninja (recommended)
- GCC 13+ or Clang 17+ with C++23 support
- Linux: `libx11-dev libxext-dev`

## Setup

```bash
# Source the dev environment (creates gk function)
source scripts/setup.sh    # Linux/macOS
. .\scripts\setup.ps1      # PowerShell
```

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

## VS Code Tasks (Optional)

Create `.vscode/tasks.json` to run builds from the editor:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build Debug",
      "type": "shell",
      "command": "python3",
      "args": ["${workspaceFolder}/scripts/cli.py", "build", "debug"],
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": "$gcc"
    },
    {
      "label": "Run Tests",
      "type": "shell",
      "command": "python3",
      "args": ["${workspaceFolder}/scripts/cli.py", "test"],
      "problemMatcher": []
    }
  ]
}
```

Run with: `Ctrl+Shift+B` (build) or `Ctrl+Shift+P` → `Tasks: Run Task`
