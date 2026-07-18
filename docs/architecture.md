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

The game API is a flat, versioned function table containing fixed-width values, pointers, and callbacks. Gecko's normal C++ API is available because the game links Gecko, but reload/version negotiation stays at this narrow boundary. Add compatibility checks before loading released games built against multiple engine versions. Nonbreaking engine additions keep the existing game API version; a changed layout or contract increments it.

Subsystem boundaries are directories for navigation, not separately shipped libraries. Calls inside Gecko are ordinary direct calls. Function tables exist only where a real runtime boundary needs them: game/plugin loading or selecting an OS/graphics backend. A final monolithic build can compile the same game implementation into the executable and call the same API without dynamic loading.

Memory belongs to Gecko. The shared library owns the allocator state, and every binary uses the exported allocation functions. OS allocation is the bottom layer. Graphics resource allocation is explicit Vulkan allocation. No third-party allocator or container library is part of the engine.

Initialization and shutdown are explicit. The long-term public shape is `Initialize(config)` / `Shutdown()` with typed result enums. Destructors may clean up small local values, but correctness must not depend on global destructor order, exception unwinding, or hidden ownership chains.

Wayland is preferred on Linux and X11 remains available for compatibility. Windows uses Win32. Platform selection happens once during initialization. Vulkan is required for hardware rendering; a null backend is useful for headless operation and a software renderer can be added as another explicit backend.
