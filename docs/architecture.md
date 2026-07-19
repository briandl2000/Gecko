# Architecture

Gecko has one process-wide engine instance and one shared engine library. It owns memory, logging, profiling, jobs, events, platform state, and graphics state. These are concrete Gecko systems with small configuration structs, not injected interface graphs.

The shipping shape is:

```text
gecko_launcher ─┐
editor/tool exe ├── Gecko shared library ── OS + Vulkan
game shared lib ┤
plugin shared lib ┘
```

The launcher loads a game library with the platform shared-library functions and asks for exactly one known symbol, `GeckoGame_GetApi`. A game/plugin is compiled and linked against the same Gecko import library/shared object as the launcher. It does not search for every Gecko symbol itself and it does not embed a static copy of the engine.

The game API is a flat, versioned function table containing fixed-width values, pointers, and callbacks. Gecko's normal C++ API is available because the game links Gecko, but reload/version negotiation stays at this narrow boundary. The loader checks both the game-table version and the engine ABI version. Compatible additions keep their existing numbers; a broken C++ boundary increments `EngineAbiVersion`, while a broken game-table contract increments `GameApiVersion`.

Subsystem boundaries are directories for navigation, not separately shipped libraries. Calls inside Gecko are ordinary direct calls. Function tables exist only where a real runtime boundary needs them: game/plugin loading or selecting an OS/graphics backend.

The engine uses a deliberate unity build. Each subsystem has one unity source that includes its implementation files, and `src/gecko_engine.cpp` combines those subsystem units into the shared library. This keeps the build visible and makes internal name collisions real problems to fix instead of relying on accidental translation-unit isolation. Projects may include the complete public API through `<gecko/gecko.h>` or choose granular headers when parse time matters.

Memory belongs to Gecko. The shared library owns the allocator state, and every binary uses the exported allocation functions. OS allocation is the bottom layer. Graphics resource allocation is explicit Vulkan allocation. No third-party allocator or container library is part of the engine.

Logging, profiling, jobs, and events are exported as direct functions backed by that same state. Their concrete types are private to the engine. There is no public service registry, null-service hierarchy, or implementation injection point; `GeckoConfig` only adjusts the behavior of the one implementation.

Initialization and shutdown are explicit through `Initialize(config)` / `Shutdown()` and a typed initialization result. Destructors may clean up small local values, but correctness must not depend on global destructor order, exception unwinding, or hidden ownership chains.

Linux currently links the OS-facing libc/pthread/math/compiler ABI surface, and Windows uses the platform/compiler startup runtime. Gecko does not link the C++ standard library on Linux and does not use its containers, ownership, formatting, I/O, or threading APIs. Removing more of the remaining platform runtime is a deliberate later project, not a hidden claim.

Wayland is preferred on Linux and X11 remains available for compatibility. Windows uses Win32. Platform selection happens once during initialization. Vulkan is required for hardware rendering; a null backend is useful for headless operation and a software renderer can be added as another explicit backend.
