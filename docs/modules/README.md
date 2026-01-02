# Modules

## Core (`gecko::Core`)
Owns foundational interfaces and utilities:
- services install/access
- allocator interface + helpers
- job system interface + helpers
- logging/profiling interfaces and macros

Design intent: Core stays usable even without Platform/Runtime.

## Platform (`gecko::Platform`)
Owns OS abstraction and handles that must be implemented per-platform.

Near-term targets:
- window creation + event pump
- input (keyboard/mouse)
- timers/filesystem where platform differences matter

## Runtime (`gecko::Runtime`)
Owns concrete implementations that depend on Core:
- thread-pool job system
- ring logger/profiler
- sinks (console/file/trace)
- tracking allocator

Design intent: Runtime is “batteries included”, but swappable.
