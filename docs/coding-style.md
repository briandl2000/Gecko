# Coding style

Gecko uses C++26 as a better C: data and control flow should remain visible, layouts should be understandable, and costs should be predictable.

- Use namespaces, `auto`, `constexpr`, enum classes, designated initialization, lambdas, concepts, and small templates when they make code clearer.
- Templates belong in foundation containers, math, and compile-time utilities. Avoid template architecture, type-erased ownership webs, and error messages spanning the program. Keep implementation-only templates in private headers.
- Public runtime boundaries use plain structs, fixed-width Gecko types, pointers, counts, enums, and function pointers. Do not expose standard-library ownership or containers across the shared-library boundary.
- No exceptions or RTTI. Builds disable both. Expected failures return a typed result enum or a result struct. Assertions are for programmer errors and invariants, not ordinary runtime failure.
- Prefer explicit `Initialize` / `Shutdown`, `Create` / `Destroy`, and caller-visible ownership. RAII is fine for a small lexical guard when it cannot hide important lifetime or allocation behavior; it is not the engine architecture.
- One Gecko implementation exists for allocator, logger, profiler, jobs, and events. Configuration changes behavior; dependency injection does not choose implementations.
- Core, Math, Platform, Graphics, and Debug Renderer are explicit engine modules. A `module.py` may contribute a unity object or produce an executable/plugin. Module boundaries express build order and ownership; they do not imply another shipped engine library.
- Configuration is plain data with default member initializers. Nest module configs by value in the owning config; do not add registries, inheritance, callbacks, or generic option bags.
- Allocation is never implicit in a low-level API. A function that may allocate should make that clear through its owning type, allocator/arena, or documentation at the declaration.
- Virtual dispatch is reserved for an actual runtime-polymorphic boundary. Ordinary subsystem code uses concrete types and direct calls.
- Include public Gecko headers as `"gecko/..."`. Private files use an unambiguous relative path. Include lookup must never depend on directory ordering.
- Inside `namespace gecko`, use unqualified names or `platform::Name`. From other code use `gecko::Name`. Use leading `::` only for genuinely global external APIs when it prevents ambiguity: `::vkCreateDevice`, `::wl_display`, `::XOpenDisplay`, `::CreateWindowExW`. Never write leading `::gecko::`.
- Engine code does not use the C++ standard library for containers, ownership, formatting, I/O, threading, or runtime services. Compiler, OS, Vulkan, and the small C math/compiler-runtime surface used by the platform layer are explicit exceptions.
- User-facing text uses Gecko's `{}` formatter. Prefer `"name={} size={:08x}"` style placeholders over printf-style `%` formatting. `FormatTo` writes into caller-owned storage; logging and assertion formatting use fixed local buffers.
- Types and functions use `PascalCase`; local variables use `camelCase`; public data fields use `PascalCase`; private fields use `m_`; globals use `g_`; macros use `GECKO_...`.
- Constants use `PascalCase` without a `k` prefix. Public header constants are `inline constexpr`; file-local constants stay in the narrowest namespace; class constants are `static constexpr`. Mutable globals retain `g_`.
- Use a macro only when the language cannot replace it: platform/compiler selection, symbol export, call-site capture for assertions/logging/profiling, or compile-out behavior. Prefix project macros with `GECKO_`. Never hide algorithms or control flow in macros.
- Tracked source, build, configuration, and documentation files are ASCII-only unless a file is explicitly intentional UTF-8 test or content data.
- Braces go on their own line. Keep functions focused. Comments explain reasons, contracts, and non-obvious platform behavior, not the syntax.

Assertions call Gecko's own failure handler with expression, optional message, source file, line, and function. Messages may use the same typed placeholders: `GECKO_ASSERT(index < count, "index {} exceeds {}", index, count)`. Debug builds break into an attached debugger and terminate; release assertions compile out. `GECKO_VERIFY` always evaluates its expression. Never throw from an assertion or convert expected OS/Vulkan errors into assertions.
