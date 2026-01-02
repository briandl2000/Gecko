# Documentation Roadmap

Goal: make it easy for *future you* (and contributors) to understand how to use Gecko, how to extend it, and how to validate changes.

## Phase 1 — Make the basics obvious
- Add a docs index (done: [docs/README.md](../README.md))
- Document build + install/export usage
- Document module boundaries (Core/Platform/Runtime)

## Phase 2 — “How to use Gecko” guides
- “Services & Boot”: how to install custom allocators/job system/logger/profiler
- “Logging”: categories, sinks, threading expectations, performance notes
- “Profiling”: trace output format, how to view in Chrome tracing, counters vs zones
- “Memory”: allocator contract, tracking allocator, best practices for wrappers

## Phase 3 — “How to extend Gecko” guides
- Platform layer: adding a new platform implementation
- Rendering layer: adding a new backend (Vulkan first)
- UI layer: integrating UI with rendering/input
- ECS: how systems schedule jobs and interact with memory/profiling

## Phase 4 — Engineering docs
- Testing strategy (unit + integration)
- Debugging playbook (ASAN/UBSAN/TSAN, logging/profiling recipes)
- Performance guidelines (hot paths, allocations, job granularity)

## Deliverables (recommended file set)
- `docs/architecture/overview.md` (done)
- `docs/build.md` (done)
- `docs/modules/README.md` (done)
- `docs/services.md`
- `docs/logging.md`
- `docs/profiling.md`
- `docs/memory.md`
- `docs/platform/windowing.md`
- `docs/rendering/abstraction.md`
- `docs/rendering/vulkan_backend.md`
- `docs/ui.md`
- `docs/ecs.md`
