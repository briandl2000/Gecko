# Project Guide / Roadmap

This is the single place to answer:

- What should I work on next?
- How do I stay organized?
- How do I use AI without becoming dependent?

Gecko is a hobby project.
Goal: **good architecture** + **steady progress**.

---

## 1) North Star

Gecko is a modular engine split into:

- **Core**: foundational interfaces + utilities (services, allocator/jobs/log/profiler)
- **Platform**: OS abstraction boundary (window, input, filesystem primitives)
- **Runtime**: concrete implementations built on Core (+ uses Platform when needed)

**Rule of thumb:** Core stays small and portable; Platform isolates OS conventions; Runtime is “batteries included”.

---

## 2) The workflow (simple and repeatable)

### The weekly loop

1. **Pick one theme** (one milestone section below)
2. Pick **1–3 tickets** only
3. Implement + validate (build + run at least one example)
4. Write a short note: what changed + why
5. Move tickets to Done, and pick next ones

### Definition of Done (DoD)

A ticket is “done” when:

- Builds in Debug + Release
- The relevant example runs (or a new minimal example exists)
- Public API changes are reflected in docs (at least the hub + this file)
- You can explain the change without reading AI output

---

## 3) “Write it myself” guardrails (AI boundaries)

AI is allowed, but you stay in control.

Allowed uses:
- explore unfamiliar APIs (X11/Wayland/Vulkan details)
- generate small snippets that you rewrite/verify
- review for edge cases and missing tests
- draft docs (that you edit)

Avoid:
- pasting large generated subsystems without understanding
- accepting large refactors “because it compiles”

Personal rule that works well:
- For any non-trivial change, write a **2–5 sentence design note** (in the issue/card) before coding.

---

## 4) Roadmap (milestones)

### Version scheme
- **v0.0.0-alpha.N** - Alpha releases (N = 0, 1, 2, ...)
- **v0.0.0-beta.N** - Beta releases
- **v0.1.0** - First stable release

### v0.0.0-alpha.0 (Core foundation)
**Status:** In progress

**Goals:**
- Core refactor (organize by subsystem)
- Math module (header-only, no IModule)
- Event system ✅

### v0.0.0-alpha.1 (Platform layer)
**Status:** Planned

**Goals:**
- Platform: windowing, input, file I/O
- Platform threading interface (Thread, Mutex primitives)
- Runtime: refactor job system to use platform threading
- Platform capabilities/info system

**Key decision:** Single-threaded rendering is fine for 2D - no render thread needed yet.

### v0.0.0-alpha.2 (Graphics)
**Status:** Planned

**Goals:**
- Graphics module with Vulkan backend
- Simple 2D renderer (sprite batching, immediate submission)
- Resource loading using existing job system

### v0.0.0-beta.0 (Production validation)
**Status:** Planned

**Goals:**
- Build simple 2D game (clone existing game for validation)
- Box2D physics wrapper (abstracted interface)
- Iterate based on real usage feedback

### v0.1.0 (First stable release)

**Status:** Future

**Goals:**
- Add render thread when 3D complexity demands it
- 3D rendering features
- Physics, audio modules

---
## v0.0.0-alpha.0 release target (what “done” means)

This repo is in active development. For the first tagged alpha, treat this as the intended scope.

### Ticket specs (recommended)

These are sized to be trackable as separate cards/issues. If a ticket grows beyond ~1–2 evenings, split it.

#### Ticket: Platform windowing

**Description**
- Provide a minimal window API through `gecko::platform::PlatformContext` that works end-to-end on Linux.

**Checklist**
- Create/destroy window.
- `PumpEvents()` + `PollEvent()` produce window lifecycle events.
- `CloseRequested` event and `RequestClose()` work.
- Backend selection (`WindowBackendKind::Auto`) chooses a working backend or fails cleanly.
- Getting and setting window info through PlatformContext using the handle.

**Definition of Done**
- `platform_example` opens a window, pumps events, and exits cleanly.
- Failure paths return `false` (no crashes) and log a useful message.

#### Ticket: Platform IO

**Description**
- Provide a minimal IO API suitable for tools and games (read/write whole files + basic path helpers).

**Checklist**
- Read whole file (binary + text) with explicit error result.
- Write whole file (binary + text) with explicit error result.
- Path helpers: join/normalize + exists/is-file/is-dir.
- At least one “special directory” query (executable dir OR user data dir).

**Definition of Done**
- Linux implementation works and has a small example usage path.
- Ownership rules are clear (allocator/buffer lifetime is explicit).

#### Ticket: Platform input

**Description**
- Deliver keyboard/mouse input via the same event loop as window events.

**Checklist**
- Keyboard key down/up/repeat.
- Mouse move, button down/up, wheel.
- Events are associated with a window (or document why not).

**Definition of Done**
- `platform_example` can log input events.
- Event delivery is stable under normal usage.

#### Ticket: Decide on event system

**Description**
- Pick the engine-wide “event system” shape so Platform events integrate cleanly with future engine/game systems.

**Checklist**
- Decision recorded: queue vs callbacks vs pull-based polling.
- Threading rules decided (main-thread only vs cross-thread enqueue).
- Lifetime/ownership decided for payloads.
- Minimal types defined (tag + payload) without allocations in the hot path.

**Definition of Done**
- There’s a clear written decision.
- Platform events either route through it, or it’s explicitly deferred with rationale.

#### Ticket: Platform Monitor System

**Description**
- Provide monitor enumeration for sane defaults (DPI, fullscreen selection, window placement).

**Checklist**
- Enumerate monitors.
- Primary monitor query.
- DPI/scale info where available.
- Work area and/or current mode where available.

**Definition of Done**
- Linux implementation works for at least one backend.
- API clearly indicates what can be unavailable.

#### Ticket: Release system / pipeline

**Description**
- Make releases repeatable: tag → CI builds → SDK artifacts attached to a GitHub Release.

**Checklist**
- `cmake --install` produces a complete SDK (headers + libs + CMake config).
- Decide artifact layout (include/lib/lib/cmake/Gecko).
- Add CI that builds + installs + packages artifacts for at least Linux.
- Add release checklist (what to verify before tagging).
- Document the workflow: branching, version bump, tagging.

**Definition of Done**
- Tag `v0.0.0-alpha.0` produces downloadable artifacts.
- A tiny external consumer can `find_package(Gecko CONFIG)` and link successfully.

See the detailed workflow doc: [docs/release-workflow.md](release-workflow.md)

