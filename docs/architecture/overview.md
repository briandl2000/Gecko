# Architecture Overview

Gecko is organized as three modules with clear layering:

- **Core**: foundational services and utilities (allocator, jobs, profiler, logger, time/thread/random, etc.)
- **Platform**: OS abstraction boundary (platform context; windowing/input/filesystem planned)
- **Runtime**: higher-level production implementations built on Core (thread-pool job system, ring logger/profiler, sinks, tracking allocator)

## Service Model
Most systems are accessed via a global “installed services” table:
- `IAllocator*`, `IJobSystem*`, `IProfiler*`, `ILogger*`

This enables:
- modular swapping of implementations
- predictable initialization/shutdown order
- no hard dependency on heavyweight subsystems in Core

### Dependency Order
Recommended install order:
1. Allocator
2. Job system
3. Profiler
4. Logger

And shutdown in reverse.

## Design Goals
- Keep Core small and dependency-light.
- Put platform and rendering behind interfaces to preserve portability.
- Prefer data-oriented + profiling-first workflows (categories everywhere).
