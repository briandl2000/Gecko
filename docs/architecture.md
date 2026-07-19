# Architecture

Gecko is one shared library and one process-wide engine instance. Any executable may link it directly. A host may also load zero, one, or many shared-library plugins; a game is one possible plugin role.

```text
standalone executable ----+----> Gecko shared library ----> OS + Vulkan
editor / launcher --------+
        |
        +----> game plugin --------+
        +----> tool plugin --------+----> same Gecko shared library
        +----> other plugins ------+
```

Every executable and plugin in a process links the same `Gecko.dll` or `libGecko.so`. Gecko therefore owns one allocator and one set of logging, profiling, job, event, platform, and graphics state.

The host resolves `GeckoPlugin_GetApi` from a plugin. The returned function table uses fixed-width values, pointers, and callbacks. During alpha development, `PluginApiVersion` and `StructSize` protect this loader contract; plugins are otherwise rebuilt with the engine.

## Build concepts

A module is a unit of code, dependencies, and resources described by `module.py`. A project is the root module selected for a build. A plugin is a module packaged as a shared library; an executable is a module packaged as a program. A source module may instead contribute code to its requesting project.

The engine itself is the root project producing the Gecko shared library. Core, Math, Platform, Graphics, and Debug Renderer are source or header modules with the same `module.py` contract used by external projects. Their dependencies are resolved topologically and a cycle stops the build before compilation. Each source module compiles its own unity file; the resulting objects are linked into one Gecko shared library, not separate engine libraries.

The `src/module.py` descriptor makes the source tree itself the explicit engine project: it names the final engine glue source and selects the transitive module graph. One unity object per source module is the middle ground between a single giant translation unit and per-file compilation: clean builds stay small, changes normally rebuild only the affected module, and include-order accidents cannot leak between subsystems.

Module descriptions are ordinary typed Python calls imported from `build.py`. They name a unity source, dependencies, includes, shaders, and platform-specific compile and link requirements. Compiler selection, tool invocation, output layout, shader embedding, and incremental checks remain in the single root `build.py` driver. Keeping that entrypoint at the root also makes it the one build file copied into the downloadable SDK.

Successful builds generate the ignored `compile_commands.json` from the compiler commands actually used. Zed/clangd consumes that file, so no tracked editor-only `compile_flags.txt` needs to mirror the build configuration.

Shaders have logical names scoped to their module. The driver invokes `glslc -mfmt=c` and generates `<module>/Shaders.generated.h`; generated paths and symbols cannot collide across modules. Runtime shader loading can later use the same declarations without changing the Release embedding path.

## SDK boundary

The downloadable SDK contains `build.py`, public headers, a prebuilt Gecko module description, and Debug/Release libraries. The same module description builds against a source checkout or an unpacked SDK. CI stages the exact SDK directory, uses its copied driver and public artifacts to rebuild the sandbox consumer, runs it headlessly, and only then packages it.

Configuration uses plain value structs with default member initializers. Module settings stay nested by value in `GeckoConfig`; there is no config registry, dependency injection graph, or interchangeable core service hierarchy.

Initialization and shutdown are explicit through `Initialize(config)` and `Shutdown()`. Errors use typed results where callers can recover; assertions are for programmer errors. Important lifetime, allocation, and control flow must remain visible.

Linux prefers Wayland and keeps X11 for compatibility. Windows uses Win32. Vulkan is the hardware renderer, Null supports headless work, and a software renderer can become another explicit backend.
