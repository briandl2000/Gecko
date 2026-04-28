# Modules, Services, Contexts

This document locks the mental model behind Gecko's runtime structure.
If you're touching `IModule`, the service registry, `Engine`, or thinking
about how to add a new subsystem, read this first.

## Layering

```
Core  ←  Platform  ←  Runtime  ←  Program
                                  (your app, plugins)
```

- **Core** declares interfaces, value types, and utilities. No global
  state of its own beyond a few atomics that live in *CoreServices*.
- **Platform** depends on Core. Wraps OS surfaces (windows, monitors,
  IO, library loading).
- **Runtime** depends on Core and may use Platform. Provides concrete
  implementations of Core's service interfaces (logger, profiler, job
  system, event bus, allocator).
- **Program** is the consumer: an executable or a plugin. Constructs
  module instances, hands them to `Engine::Create`, runs the app.

Runtime depends on Platform because real implementations (e.g. ring
file logger, threaded job system) need OS facilities. Platform must
not depend on Runtime — that's why services like the *plugin loader*
live in Platform, while *threaded job system* lives in Runtime.

## Five Concepts

| Concept   | Lifetime           | Owner       | Crosses module boundary | Example                          |
| --------- | ------------------ | ----------- | ----------------------- | -------------------------------- |
| Library   | Program-wide       | Build sys   | n/a (source artifact)   | `GeckoCore`, `GeckoPlatform`     |
| Service   | Engine boot-time   | User code   | Yes                     | `ILogger`, `IJobSystem`          |
| Module    | Engine boot-time   | User code   | n/a (publishes services)| `CoreServicesModule`, `PlatformModule`|
| System    | Module-internal    | Module      | No (private)            | Job dispatcher inside Runtime    |
| Context   | Scoped (RAII)      | User code   | No (handle, not service)| `PlatformContext`                |

### Library

A CMake target. Static, shared, or interface. May contain zero or one
`IModule` lifecycle nodes. **Math** is INTERFACE (header-only, no
state). **Core** has no `IModule` — its lifecycle is owned by `Engine`
itself. **CoreServices** is `SHARED` so the registry, allocator atomic,
and `Engine` storage live in one place that every DSO links against
(this is the basis of the future plugin model).

### Service

A pure-virtual interface declared in Core (`I*`), plus a `Null*`
default and a `ServiceIdOf<T>()` consteval ID. Concrete implementations
live wherever it makes sense — usually Runtime or Platform.

Services are **published** by exactly one module (its `Publishes()`
list) and **required** by zero or more (its `Requires()` list). The
registry uses these declarations to topo-sort modules at startup.

```cpp
class ILogger : public IService { ... };
class NullLogger : public ILogger { ... };          // Core
class RingLogger final : public ILogger { ... };    // Runtime impl
class FileLogger final : public ILogger { ... };    // Runtime impl
```

User code constructs the impl, hands it to a module, the module
publishes it, other modules and `GetLogger()` resolve to the same
pointer for the engine's lifetime.

### Module

An `IModule` lifecycle node. Owns the *bookkeeping* for a library's
runtime presence: `Startup`, `Shutdown`, `Requires`, `Publishes`. It
does **not** own the service implementations — the user does. Modules
are always stack-constructed (or otherwise user-owned) and passed by
pointer to `Engine::Create`:

```cpp
runtime::ThreadPoolJobSystem  jobs;
runtime::RingProfiler         profiler {1 << 16};
runtime::RingLogger           logger {1024};
runtime::EventBus             events;

runtime::CoreServicesModule  rt {jobs, profiler, logger, events};
platform::PlatformModule plat;
MyAppModule              app;

auto engine = Engine::Create({&rt, &plat, &app});
```

Module construction patterns we deliberately *don't* use:

- **Singleton with free `GetModule()`** — historic. Removed because it
  hides ownership and prevents multiple instances per process for
  testing.
- **Module owns service impls internally** — historic. Removed because
  it forced the module's ctor to know how every service is configured.
  The user knows what they want; the module just publishes.

A library with nothing to do at runtime exports **zero** modules
(Math, Core).

### System

A worker that lives *inside* a module. Never crosses the module
boundary — not exposed via the registry, not in any header outside
`src/<module>/private/`. If you find yourself wanting a service-shaped
thing that another module needs, promote it to a service. Otherwise
keep it private.

### Context

A user-owned RAII handle that opens scoped state. Multiple contexts
may exist per process. Created via a service factory or, today, a
direct constructor.

```cpp
PlatformConfig cfg = { .Backend = DisplayBackendKind::X11 };
PlatformContext ctx(cfg);   // user owns
auto winId = ctx.Windows().Create(spec);
```

Contexts differ from services in three ways: they are **owned by
user code** (not the registry), there can be **many simultaneously**,
and they are **created on demand** rather than published once at boot.

## Engine

`Engine` is the RAII root. `Engine::Create({modules...})` constructs a
private `IModuleRegistry`, registers each module, runs Kahn's
algorithm over `Requires()` / `Publishes()` to determine startup
order, then drives `Startup`. Destruction reverses.

`Engine` itself is NOT an `IModule` — it is the *runner* of modules
and the lifecycle root for Core's global atomics (registry pointer,
allocator pointer). This is why Core has no `IModule` of its own.

The Engine impl lives in CoreServices (the shared library) so that any
DSO in the process resolves to the same registry pointer.

## State Ownership Summary

| State                      | Owned by             | Lives in              |
| -------------------------- | -------------------- | --------------------- |
| Service implementations    | User (stack/heap)    | User code             |
| Module instances           | User (stack)         | User code             |
| Service registry pointer   | `Engine`             | `GeckoCoreServices`   |
| Allocator pointer          | `Engine` (set/reset) | `GeckoCoreServices`   |
| Module-internal state      | `IModule` impl       | `src/<lib>/`          |
| Per-context state          | User (`Context` obj) | User code             |

The user owns what they configure. The registry owns nothing but
indirection.

## Library Output Names

CMake target names remain short for internal references; output file
names use the `Gecko` prefix to make the on-disk layout unambiguous.

| Target          | Output                       |
| --------------- | ---------------------------- |
| `Core`          | `libGeckoCore.a`             |
| `CoreServices`  | `libGeckoCoreServices.so`    |
| `Platform`      | `libGeckoPlatform.a`         |
| `Runtime`       | `libGeckoRuntime.a`          |
| `Graphics`      | `libGeckoGraphics.a`         |
| `Math`          | (interface, no output)       |

The `Gecko::*` aliases (`Gecko::Core`, `Gecko::CoreServices`, …) are
the public consumer-facing names and remain stable.

## Where Things Live

```
include/gecko/<lib>/        Public headers
src/<lib>/                  Implementation
src/<lib>/private/          Internal headers (not public API)
src/core/services/          CoreServices' impl (shared lib sources)
src/core/private/           Core / CoreServices internal headers
test/<lib>/                 Catch2 tests
```

The `IModuleRegistry` *interface* is in
[`include/gecko/core/services/modules.h`](../../include/gecko/core/services/modules.h);
the concrete `gecko::core::detail::ModuleRegistry` is private to
CoreServices and not exposed.

## Decision Tree

When adding new functionality, ask:

1. **Stateless and used everywhere?** Header-only utility in Core.
2. **Cross-module interface?** Service interface in Core; impl in
   Runtime/Platform; published by an `IModule`.
3. **Lifecycle work for a whole library?** That library's
   `IModule::Startup` / `Shutdown`.
4. **Internal worker?** System in `src/<lib>/private/`.
5. **Per-call user-owned state?** `Context` class.

If you find yourself making a service that only one consumer ever
uses, it's a system. If you find yourself wanting two of a service in
the same process, it's a context.

## Deferred / Open

- **Plugin loader**: Platform-side `ILibraryLoader` service for
  `dlopen`/`LoadLibrary`. Lands when there's an actual plugin.
- **Plugin ABI rule**: today plugins must be built with the **same
  compiler and same C++ ABI** as the host (one toolchain per process).
  Vtable layout (Itanium vs MSVC ABI) and exception model (DWARF vs
  SEH) are implementation-defined and not stable across toolchains.

  What we *do* control across toolchains is the data shapes that cross
  the `CoreServices` shared-library boundary. The rule on every virtual
  in `include/gecko/core/services/*.h`:

  - Allowed parameter / return types: primitives, raw pointers,
    `const char*`, Gecko PODs, and `gecko::Span<T>`
    (see [`include/gecko/core/span.h`](../../include/gecko/core/span.h)).
  - **Forbidden**: any `std::` type whose layout is implementation-defined
    (`std::string`, `std::string_view`, `std::span`, `std::vector`,
    `std::function`, ...). Use the Gecko POD equivalent or pass a raw
    pointer + size.
  - Free functions in static libraries (`Core` impl, `Runtime` impl,
    `Platform` impl) may use `std::` types freely -- the rule is
    boundary-local.

  Verified by [`scripts/lint_abi.py`](../../scripts/lint_abi.py), wired
  into `gk test` (it runs as a pre-step before the test executables).
  CI does not currently invoke `gk test`; it builds and runs test
  binaries directly, so the lint is enforced locally via `gk test`
  rather than by GitHub Actions today. A future cross-toolchain plugin would also
  need a separate C ABI shim (`extern "C" gecko_plugin_register(...)`
  with POD-only types and function pointers); that's deferred until a
  plugin loader exists to validate it.
- **Core-as-shared**: Currently `Core` is `STATIC` and `CoreServices`
  is `SHARED`. The split is plugin-compatible: plugins link `Core`
  statically (utilities, no globals) and `CoreServices` dynamically
  (shared singletons). Merging into one shared library is possible
  but unnecessary today.
- **Hot-swap services**: Deferred indefinitely. Service pointers are
  intended to be set once at startup.
- **Dynamic module reload**: Deferred. When implemented, dependents
  must shut down before a module is unloaded.
