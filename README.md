# Gecko Game Engine

A modular, high-performance C++ game engine designed as a collection of interconnected systems rather than a monolithic framework.

## About This Project

Gecko began as a graphics rendering engine during my graduation project at [BUAS](https://www.buas.nl/), but has evolved into a comprehensive game engine ecosystem. The original graphics engine (preserved in the `archive/graphics-engine-v1` branch) focused on PBR rendering and cross-platform graphics, but the current `main` branch represents a complete architectural redesign with a focus on modularity, performance, and developer experience.

## Architecture Overview

Gecko is built around three core modules that work together:

### **Core** - Foundation Systems
- **Memory Management**: Custom allocators with tracking and profiling
- **Service Architecture**: Dependency-ordered service initialization  
- **Job System**: Multi-threaded task scheduling with dependencies
- **Logging**: High-performance ring-buffer logging with multiple sinks
- **Profiling**: Low-overhead profiling with Chrome tracing output
- **Utilities**: Threading, timing, random generation, and bit manipulation

### **Platform** - System Abstraction
- **Platform Context**: Cross-platform system integration
- **Window Management**: In progress (target: v0.0.0-alpha.0)
- **Input Handling**: In progress (target: v0.0.0-alpha.0)
- **Platform IO / File System**: In progress (target: v0.0.0-alpha.0)
- **Monitor System**: In progress (target: v0.0.0-alpha.0)

### **Runtime** - Advanced Components
- **Thread Pool Job System**: Production-ready multithreading
- **Ring Logger/Profiler**: Lock-free, high-performance data collection
- **Tracking Allocator**: Memory usage analytics
- **Crash-Safe Tracing**: Reliable profiling data even during crashes
- **Multiple Sink Types**: Console, file, and custom output targets

## Design Philosophy

- **Modular**: Each system can be used independently or together
- **Performance-First**: Lock-free data structures, cache-friendly design
- **Developer-Friendly**: Comprehensive logging, profiling, and debugging tools
- **Cross-Platform**: Designed for Windows, Linux, and console platforms
- **Service-Oriented**: Clear dependency management and initialization order

## Quick Start

```cpp
#include "gecko/core/boot.h"
#include "gecko/core/labels.h"
#include "gecko/core/services.h"
#include "gecko/core/services/log.h"

namespace app::my_game::labels {
inline constexpr gecko::Label Game = gecko::MakeLabel("app.my_game");
}

int main() {
    // Set up core services
    gecko::Services services{};
    GECKO_BOOT(services);  // Installs default services
    
    // Your game code here
    GECKO_INFO(app::my_game::labels::Game, "Hello from Gecko!");
    
    GECKO_SHUTDOWN();
    return 0;
}
```

## Examples

- **`core_example`**: Demonstrates memory management, job system, logging, and profiling
- **`platform_example`**: Shows platform abstraction usage

## Building

Gecko uses CMake and requires C++23:

```bash
git clone https://github.com/briandl2000/Gecko.git
cd Gecko
cmake --preset debug    # or release
cmake --build build --config Debug
```

### Prerequisites
- **CMake 3.22+**
- **C++23 compatible compiler** (GCC 13+, Clang 16+, MSVC 2022)
- **Ninja** (recommended)

### Platform-Specific Notes
- **Windows**: Requires MSVC with latest Windows SDK
- **Linux**: GCC or Clang with C++23 support

## Documentation

- [**Documentation Index**](docs/README.md)
- [**Project Guide / Roadmap**](docs/project-guide.md)
- [**Coding Standards (reference)**](docs/CODING_STANDARDS.md)
- **Examples**: See `examples/` directory for usage patterns
- **Headers**: Extensive inline documentation in `include/gecko/`

## Project Status

**In Active Development.** v0.0.0-alpha.0 is intended to cover Platform MVP + a minimal app skeleton loop + install/docs sanity.
Current priorities and next steps are tracked in the single canonical roadmap:
- [docs/project-guide.md](docs/project-guide.md)

## Repository History

The original graphics engine version is preserved in the `archive/graphics-engine-v1` branch. The current `main` branch represents the new direction with completely rewritten architecture and expanded scope.

## License

MIT License - see [LICENSE](LICENSE) for details.