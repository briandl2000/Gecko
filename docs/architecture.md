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
