# Gecko Core Containers, Allocators, and Formatting Plan

## Current Situation

Gecko already has a clear architectural split between public module/service boundaries and implementation internals. `include/gecko/core/span.h` explicitly exists to avoid `std::span` on the `CoreServices` shared-library boundary, but that rule is not yet applied consistently to strings, byte buffers, platform I/O, graphics APIs, and some runtime implementation classes.

The near-term concern is not a broad C ABI. Gecko's engine-facing code is C++, and that is fine. The practical problem is that public engine APIs, module interfaces, and loaded shared objects may be built with different compiler versions, standard-library versions, or build flags. Exposing Gecko-owned data structures gives the engine more control over layout, allocation, version checks, and migration. A C ABI can be designed later for specific language/game-code integration points.

The allocator system is process-global infrastructure, not a normal service. `gecko::Allocator()` returns either a user-installed allocator from `SetAllocator()` or a process-lifetime `SystemAllocator`. `TrackingAllocator` already uses a thread-local label stack, so the codebase has precedent for thread-local scoped allocation context, but there is not yet a thread-local allocator stack.

The current formatting branch has experimental headers:

- `include/gecko/core/utility/string.h`: aliases `gecko::String` to `std::string` and `gecko::StringView` to `std::string_view`.
- `include/gecko/core/utility/format.h`: wraps `std::format`/`std::vformat` and returns that aliased `String`.

Those aliases are useful as a sketch but should not become the long-term public model because they preserve the same ABI/allocation problems under Gecko names.

## Categorized `std` Usage

### Safe / Private

These are mostly acceptable implementation details, especially before Gecko containers exist:

- `src/core/services/module_registry.cpp`: `std::unordered_map` and `std::vector` for registry bookkeeping.
- `src/core/services.cpp`: `std::atomic` for global service pointers and `std::string`/`std::unordered_map`/`std::mutex` for profiler thread-name storage.
- `src/platform/linux/platform_io_linux.cpp` and `src/platform/win32/platform_io_win32.cpp`: temporary `std::string`/`std::vector` for OS conversion and file reads.
- `src/graphics/vulkan/vulkan_device.cpp`: many temporary `std::vector` allocations for Vulkan enumeration and staging.
- Backend/private headers under `src/platform/*` and `src/graphics/vulkan/*`: `std::vector`, `std::string`, `std::unordered_map`, `std::mutex`, `std::thread::id` for backend state.
- Tests and examples: broad `std` use is fine unless the example is meant to demonstrate public API style.

### Questionable

These are public headers, but mostly concrete implementation classes rather than pure ABI interfaces:

- `include/gecko/runtime/ring_logger.h`: exposes `std::vector`, `std::mutex`, and `std::atomic` in public class layout.
- `include/gecko/runtime/ring_profiler.h`: exposes `std::vector`, `std::string`, `std::unique_ptr`, `std::mutex`, and `std::atomic` in public class layout.
- `include/gecko/runtime/thread_pool_job_system.h`: exposes `std::thread`, `std::condition_variable`, `std::priority_queue`, `std::shared_ptr`, `std::unordered_map`, and `std::vector`.
- `include/gecko/runtime/event_bus.h`: exposes `std::unique_ptr` to STL containers and synchronization members.
- `include/gecko/runtime/tracking_allocator.h`: exposes `std::unordered_map`, `std::mutex`, and `std::atomic`; `Snapshot(std::unordered_map<...>&)` is a real API leak.
- `include/gecko/core/ptr.h`: aliases `Unique`, `Shared`, and `Weak` to standard smart pointers. This improves call sites but does not solve ABI, allocation, or cross-toolchain issues.
- `include/gecko/core/engine.h`: `Engine::Create()` returns `std::optional<Engine>` and stores `std::unique_ptr<IModuleRegistry>`. The file already documents this as ABI-ok only under the same-toolchain-per-process rule.

### Should Probably Be Replaced / Wrapped

These are public API signatures that allocate or borrow using standard-library types:

- `include/gecko/platform/platform_io.h`:
  - `DirEntry::Name` is `std::string`.
  - `ReadResult` owns `std::vector<std::byte>`, returns `std::span`, and exposes `Take()` as `std::vector`.
  - `DirIter::Next()` returns `std::optional<DirEntry>`.
  - `FileWriter::Write(std::span<const std::byte>)` and `WriteString(std::string_view)`.
  - `Read`, `Write`, `AtomicWrite`, `Map` use `std::span`.
  - `ExePath`, `WorkingDir`, and `UserDataDir` return `std::string`.
- `include/gecko/platform/clipboard.h`: `GetClipboardText()` returns `std::string`, `SetClipboardText()` accepts `std::string_view`.
- `include/gecko/platform/terminal.h`: `Print`/`PrintLine` accept `std::string_view`.
- `include/gecko/platform/input.h`: `IInput::GetTypedText()` and `platform::GetTypedText()` return `std::string_view`.
- `include/gecko/platform/path_view.h`: `PathView` stores `std::string_view` and accepts `std::string`.
- `include/gecko/graphics/command_list.h`, `include/gecko/graphics/graphics_device.h`, and `include/gecko/graphics/graphics_types.h`: graphics virtual APIs use `std::span` for push constants, frame presentation, timestamp reads, uploads, and shader bytes.
- `include/gecko/core/utility/string.h` and `include/gecko/core/utility/format.h`: the new aliases/wrappers should be redesigned before use spreads.

### ABI / Public-Header Risk

Highest-risk items if Gecko moves toward loaded modules, shared libraries, or mixed compiler/STL versions:

- Any `GECKO_API virtual` method using `std::span`, `std::string_view`, `std::optional`, `std::string`, `std::vector`, `std::unique_ptr`, or `std::shared_ptr`.
- Public owning return values allocated by callee and destroyed by caller, especially `std::string`, `std::vector`, `std::optional<Engine>`, and smart pointers.
- Public classes with STL members in their object layout when constructed in one binary and used/destroyed in another.
- `std::format` wrappers in headers, because `std::format_string`, `std::make_format_args`, and `std::vformat` force `<format>` and standard formatting machinery into every consumer translation unit.

### Phase 0 Public Header Audit

Current public-header `std` categories that need decisions before migration:

- **Views in public signatures**:
  - `std::span`: graphics command/device/shader APIs and platform I/O byte APIs.
  - `std::string_view`: platform terminal, input typed text, clipboard, path view, and platform I/O text helpers.
- **Owning public results**:
  - `std::string`: clipboard text, executable/working/user-data paths, directory entry names.
  - `std::vector`: `ReadResult` byte ownership and `Take()`.
  - `std::optional`: `Engine::Create`, `platform::Stat`, and `DirIter::Next`.
- **Public concrete implementation layouts**:
  - Runtime logger/profiler/job/event/trace classes expose `std::vector`, `std::mutex`, `std::atomic`, `std::thread`, `std::condition_variable`, `std::queue`, `std::deque`, `std::unordered_map`, `std::unordered_set`, `std::unique_ptr`, and `std::shared_ptr` in class layout.
  - These are not all immediate replacements. They should be PIMPL candidates if/when those concrete classes become loaded-module/shared-library boundary types.
- **Aliases that hide but do not remove `std`**:
  - `gecko::Unique`, `gecko::Shared`, and `gecko::Weak` are aliases over standard smart pointers.
  - The current formatting branch aliases `gecko::String`/`StringView` to standard string types.
- **Threading/atomic primitives**:
  - `std::atomic`, `std::mutex`, `std::thread`, and `std::condition_variable` should not be rewritten just to remove `std`.
  - They are acceptable private/concrete implementation details, but should not appear in stable binary layouts that cross a loaded-module boundary without an explicit decision.
- **Formatting**:
  - Public templated compile-time-checked formatting will need `<format>` in an opt-in header if `std::format` remains the backend.
  - Runtime formatting can be hidden behind `.cpp` implementation once Gecko `String` exists.

## Allocator Assessment

Current allocator capabilities:

- Explicit allocator passing: possible manually through `IAllocator*` or `IAllocator&`, but existing helpers mostly use `Allocator()`.
- Implicit global allocator: supported through `Allocator()`, `SetAllocator()`, and `ResetAllocator()`.
- Allocator stack/scopes: not supported for allocators. `AllocatorScope` installs a process-global allocator, not a nested/thread-local current allocator.
- Thread-local context: supported for labels in `TrackingAllocator` via `thread_local ThreadAllocContext`.
- Debug tracking/leak checking: partially supported by `TrackingAllocator` live byte/allocation/free counters and labels, but `Shutdown()` does not currently report leaks.
- Safe deallocation from original allocator: partially supported by allocation headers and magic values. `SystemAllocator` and `TrackingAllocator` can free each other's known allocations by routing to `PlatformFree`, but this is currently magic-based, not a stored allocator pointer or allocator vtable pointer.

Recommendation: owning Gecko containers should store the `IAllocator*` used at creation time. Implicit allocator lookup should happen only when constructing/allocating the owning object. Destruction/freeing should use the stored allocator.

## Proposed Architecture

### `Core/Containers`

Add non-owning views first:

- `StringView { const char* Data; usize Size; }`
- `Span<T>` already exists; continue using it and add convenience conversions only where ABI-safe.
- `ByteSpan` aliases may be useful: `using ByteSpan = Span<byte>; using ConstByteSpan = Span<const byte>;`

Add owning containers second:

- `String { char* Data; usize Size; usize Capacity; IAllocator* Allocator; }`
- `Array<T> { T* Data; usize Count; usize Capacity; IAllocator* Allocator; }`

Initial `Array<T>` should support constructors/destructors/move correctly from the start. A trivially-movable-only container is tempting, but it will either leak into API design or force a second incompatible container later. It is acceptable to add optimized paths for trivially movable/destructible types first.

### `Core/Memory`

Keep the existing global allocator as the default allocator source, but add:

- `CurrentAllocator()`: returns top of thread-local allocator stack, falling back to `Allocator()`.
- `AllocatorPushScope`: pushes an allocator for the current thread only.
- Explicit allocation overloads for lifetime-sensitive code.

Important rule: implicit current allocator is a creation-time convenience only. Once a `String` or `Array<T>` owns memory, it frees/reallocates using its stored allocator.

### `Core/Text`

Replace `utility/string.h` aliases with real Gecko text types:

- `StringView` in a small header with no `<string>` dependency.
- `String` in an owning string header depending on memory/container support.
- Optional adapters in a separate header for `std::string_view` and `std::string`.

Formatting should be a wrapper around `std::format` initially, but the boundary should be Gecko-owned:

- `String Format(StringView fmt, Args&&... args)` uses `CurrentAllocator()`.
- `String Format(IAllocator* allocator, StringView fmt, Args&&... args)` uses the explicit allocator.
- `void AppendFormat(String& out, StringView fmt, Args&&... args)` appends to an existing Gecko string using its stored allocator.

For compile-time checked formatting, a templated header will necessarily include `<format>` and expose `std::format_string` internally. Keep that in an opt-in header such as `gecko/core/text/format_std.h`. A non-template runtime-format overload can live in a `.cpp` and hide most `<format>` cost from public consumers.

## Public API Policy

Use Gecko types for C++ public APIs:

- Non-owning text: `StringView`.
- Non-owning arrays/bytes: `Span<T>` / `Span<const byte>`.
- Owning results: `String`, `Array<T>`, or explicit output buffers.
- Handles/resources: existing handle structs or `Unique<T>` only while same-toolchain C++ is assumed.

For future non-C++ language bindings or game-code shared objects that need a true C ABI, use C-compatible shapes:

- Handles, raw pointers, counts, status codes.
- Caller-provided output buffers.
- Optional explicit allocator callbacks or `IAllocator*` only if both sides agree on the C++ interface ABI.
- Do not pass `String`, `Array<T>`, `std::*`, or C++ virtual interfaces across a true C ABI boundary.

This is not the main migration target right now. The immediate target is C++ engine APIs that avoid raw `std` types at public/module/shared-library boundaries while still allowing `std` behind Gecko APIs.

## Migration Plan

### Phase 0: Notes / Design Only

- Keep this document as the design anchor.
- Do not expand the current `std::string` alias experiment.
- Add coding rules to `docs/CODING_STANDARDS.md` after the direction is approved.
- Treat the immediate goal as "Gecko C++ APIs own their boundary types", not "design the whole future C ABI now".
- Audit all `std` in public headers and classify each occurrence as one of:
  - Public API signature: replace or wrap.
  - Public concrete implementation layout: acceptable short term only if same-toolchain usage is assumed; prefer PIMPL before loaded-module boundaries.
  - Header-only template helper: allowed only with an `abi-ok` comment explaining that instantiation/allocation/freeing happens in the caller translation unit.
  - Private implementation detail: allowed, especially OS/Vulkan/std-format glue.

### Phase 1: Add Views / Types

- Add `gecko::StringView` with stable `{ const char*, usize }` layout.
- Keep `gecko::Span<T>` and migrate public `std::span` signatures to it.
- Update `PathView` either to use `StringView` internally or to become a path-specific wrapper over it.
- Add adapters in separate headers, not in the core ABI headers.

### Phase 2: Add Allocator-Aware Owning Types

- Add `String` storing `IAllocator*`.
- Add `Array<T>` storing `IAllocator*`.
- Implement move-only first if that keeps the API tight; add copy APIs explicitly (`Clone`, `Assign`, etc.) rather than accidental copying.
- Use stored allocator for all destruction/reallocation.
- Add tests for allocator provenance, cross-scope destruction, move, reserve, append, and failure behavior.

### Phase 3: Formatting Wrapper

- Replace the current branch's aliases with real Gecko `String`/`StringView`.
- Add explicit and implicit allocator overloads:
  - `Format(StringView fmt, Args&&...)`
  - `Format(IAllocator* allocator, StringView fmt, Args&&...)`
  - `AppendFormat(String& out, StringView fmt, Args&&...)`
- Internally use `std::vformat`/`std::format_to` for now.
- For logging hot paths, prefer stack-buffer or fixed-buffer formatting paths where possible, like existing logger code already does.

### Phase 4: Migrate Public APIs

Start with high-value, low-conceptual-risk replacements:

- Graphics: replace public `std::span` with `gecko::Span` in command/device/shader APIs.
- Platform terminal/input/clipboard: replace `std::string_view` returns/params with `StringView`.
- Platform I/O: replace `ReadResult`'s `std::vector` ownership with `Array<byte>` or a dedicated `Buffer`; replace `std::optional` returns with explicit result structs.
- Engine creation can remain documented for now, but add an ABI-clean alternative before plugin work: `bool Engine::Create(Span<IModule*> modules, Engine* out)` or an opaque handle-style factory.

### Phase 5: Migrate Private Internals Where Worth It

- Keep `std` in OS/Vulkan glue and tests until there is a clear benefit.
- Migrate hot-path persistent containers in runtime systems if allocator tracking, frame allocators, or allocation control matter.
- Avoid replacing `std::mutex`, `std::atomic`, and `std::thread` just to remove `std`; wrap only if the engine needs platform abstraction or ABI hiding.

## Recommended Coding Rules

- Public API signatures should not use `std::string`, `std::string_view`, `std::vector`, `std::span`, `std::optional`, or standard smart pointers unless explicitly marked `abi-ok` with the reason.
- Public implementation classes may use `std` members only if they are not intended to cross plugin/shared-library/toolchain boundaries; prefer PIMPL for long-lived runtime implementations.
- `StringView` and `Span<T>` are non-owning only.
- `String` and `Array<T>` always store the allocator used to allocate their storage.
- Implicit allocator lookup happens only when creating or growing an owning object without an explicit allocator.
- Free/destruction always uses the allocator stored in the object.
- Offer explicit allocator overloads for persistent/lifetime-sensitive results; implicit overloads are convenience for local/temporary code.
- Allocator stack/scope should be thread-local.
- Frame/temp allocations should use explicit frame/temp allocator scopes and must not escape the scope.
- Persistent asset/engine allocations should pass an explicit allocator or be constructed under a clearly named allocator scope.
- Formatting/logging should distinguish persistent formatted strings from transient log formatting.
- Thread/job code must capture allocator intent explicitly when work crosses threads; do not assume a thread-local allocator scope follows a job to a worker.
- Future C ABI/plugin boundaries should use handles, pointers, counts, status codes, and caller-provided buffers/allocators; no C++ containers.
- Engine C++ shared-library/module boundaries should use Gecko-owned view/owning types instead of raw `std` containers/views.
- Do not ban `std` from all public headers immediately. Ban it from public ABI/API signatures first, then hide public class layouts with PIMPL where needed.
- Keep standard formatting contained. Templated compile-time checked format wrappers will need `<format>` in headers; isolate them in opt-in text/format headers.

## Risks and Traps

- Renaming `std::string` to `gecko::String` via `using` creates a false sense of ABI safety.
- A process-global allocator stack would be unsafe; allocator scopes should be thread-local.
- Thread-local allocator scopes do not automatically propagate into jobs. Jobs need explicit allocator capture or worker setup.
- Containers that free through `Allocator()` at destruction will break when the current allocator differs from the creation allocator.
- A trivially-movable-only `Array<T>` can spread constraints through APIs and cause a second migration later.
- `std::format` can allocate internally before Gecko sees the final string. That is acceptable as a short-term implementation detail, but not as a hot-path guarantee.
- Returning owning objects across DLL/plugin/toolchain boundaries is the hardest case. Prefer caller-provided buffers or opaque handles there.
- Rewriting every private `std::vector` now would burn time without solving the boundary problem.
