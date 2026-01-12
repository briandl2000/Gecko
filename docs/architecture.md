# Architecture

## TL;DR

- Core = interfaces + tiny utilities
- Platform = OS boundary
- Runtime = concrete implementations
- Keep IO out of Core (avoids boot cycles)

Gecko is organized as modules with clear layering:

- **Core**: foundational services and utilities (allocator, jobs, profiler, logger, time/thread/random, etc.)
- **Platform**: OS abstraction boundary (platform context; windowing/input/filesystem)
- **Runtime**: concrete implementations built on Core (thread-pool job system, ring logger/profiler, sinks, tracking allocator)

## Service model

Most systems are accessed via a global “installed services” table:

- `IAllocator*`, `IJobSystem*`, `IProfiler*`, `ILogger*`

This enables:

- modular swapping of implementations
- predictable initialization/shutdown order
- low coupling across subsystems

### Dependency order

Recommended install order:

1. Allocator
2. Job system
3. Profiler
4. Logger

Shutdown in reverse.

## Data, IO, and paths (high level)

### Keep IO out of Core

Core has strict initialization rules and intentionally minimal dependencies.

If IO becomes a Core service, it tends to create cycles:

- IO init wants to log errors → logger not installed yet
- logger wants file sinks → sinks need IO → IO not installed yet

**Conclusion:** treat filesystem/IO as an OS abstraction (Platform), not a Core service.

## Platform threading interface

### Design philosophy

Platform provides minimal OS threading primitives. Runtime services build on these primitives.

**Layering:**
```
Platform: Thread, Mutex (wraps std::thread/OS APIs)
    ↓
Runtime: ThreadPoolJobSystem (uses platform::Thread)
    ↓
Application: Job submission, future render thread
```

**Benefits:**
- Clean separation: Platform = OS wrappers, Runtime = high-level services
- Testable: Can swap platform implementation
- Future-proof: Renderer, physics, etc. can all use platform::Thread
- Debuggable: Platform layer adds thread naming, profiling integration

**Initial scope (alpha v0.2):**
- `platform::Thread` - basic thread creation/join
- `platform::Mutex` - basic synchronization
- `platform::GetHardwareThreadCount()`
- `platform::GetCurrentThreadId()`

**Implementation:** Can use std::thread initially, optimize to OS APIs (pthread, Win32) later if needed.

## Math module (header-only)

### Why no IModule?

Math is intentionally **header-only** with no runtime initialization:

- Pure compile-time utilities (vectors, matrices, quaternions)
- No state → no Startup/Shutdown → no need for IModule boilerplate
- Foundational layer - must work before module system boots
- Examples: glm, DirectXMath, Eigen are all header-only

### SIMD strategy

**Compile-time selection:**
- Use preprocessor/constexpr to select SIMD paths at compile time
- No runtime CPU detection needed initially
- Keeps math module stateless and simple

**If runtime features needed later:**
- SIMD detection → add to Platform capabilities system
- Custom allocators → use existing allocator service
- Performance tracking → use existing profiler

### Structure

```
include/gecko/math/
  vec2.h, vec3.h, vec4.h    // Vector types
  mat4.h                     // Matrix types
  quat.h                     // Quaternion
  common.h                   // lerp, clamp, smoothstep, etc.
  simd.h                     // Compile-time SIMD implementations
```

**Usage pattern:**
```cpp
#include "gecko/math/vec3.h"
#include "gecko/math/common.h"

vec3 position = vec3(0.0f, 1.0f, 0.0f);
float distance = length(position);
vec3 normalized = normalize(position);
```

### Model storage as domains

Different data has different expectations. Use domains instead of “one folder”:

- **Install (read-only)**: shipped assets, default config
- **Project**: workspace-local state (e.g. `.gecko/`)
- **UserConfig**: settings, keybinds, editor prefs
- **UserData**: saves, user content
- **Cache**: derived data (safe to delete)
- **Temp**: short-lived scratch

### Put OS conventions in Platform; policy in Runtime/app

Platform answers: “what is the correct OS folder for config/data/cache?”

Runtime/app policy answers:

- app identity (org/product)
- editor vs game
- project root
- overrides (portable mode, CLI flags)

This keeps layering clean and avoids boot-time dependency traps.
