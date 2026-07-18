# Coding style

Gecko uses C++23 as a better C: data and control flow should remain visible, layouts should be understandable, and costs should be predictable.

- Use namespaces, `auto`, `constexpr`, enum classes, designated initialization, lambdas, concepts, and small templates when they make code clearer.
- Templates belong in foundation containers, math, and compile-time utilities. Avoid template architecture, type-erased ownership webs, and error messages spanning the program. Keep implementation-only templates in private headers.
- Public runtime boundaries use plain structs, fixed-width Gecko types, pointers, counts, enums, and function pointers. Do not expose standard-library ownership or containers across the shared-library boundary.
- No exceptions or RTTI. Builds disable both. Expected failures return a typed result enum or a result struct. Assertions are for programmer errors and invariants, not ordinary runtime failure.
- Prefer explicit `Initialize` / `Shutdown`, `Create` / `Destroy`, and caller-visible ownership. RAII is fine for a small lexical guard when it cannot hide important lifetime or allocation behavior; it is not the engine architecture.
- One Gecko implementation exists for allocator, logger, profiler, jobs, and events. Configuration changes behavior; dependency injection does not choose implementations.
- Allocation is never implicit in a low-level API. A function that may allocate should make that clear through its owning type, allocator/arena, or documentation at the declaration.
- Virtual dispatch is reserved for an actual runtime-polymorphic boundary. Ordinary subsystem code uses concrete types and direct calls.
- Include public Gecko headers as `"gecko/..."`. Private files use an unambiguous relative path. Include lookup must never depend on directory ordering.
- Inside `namespace gecko`, use unqualified names or `platform::Name`. From other code use `gecko::Name`. Never write leading `::gecko::`.
- Use leading `::` for genuinely global external APIs when it prevents ambiguity: `::vkCreateDevice`, `::wl_display`, `::XOpenDisplay`, `::CreateWindowExW`. Standard-library use is transitional and should not spread.
- Types and functions use `PascalCase`; local variables use `camelCase`; public data fields use `PascalCase`; private fields use `m_`; globals use `g_`; macros use `GECKO_...`.
- Braces go on their own line. Keep functions focused. Comments explain reasons, contracts, and non-obvious platform behavior—not the syntax.

Assertions call Gecko's own failure handler with expression, optional message, source file, line, and function. Debug builds break into an attached debugger and terminate; release assertions compile out. `GECKO_VERIFY` always evaluates its expression. Never throw from an assertion or convert expected OS/Vulkan errors into assertions.
