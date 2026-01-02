# Data & Storage in Gecko (IO, Paths, and “Where Does This File Go?”)

This document captures design notes around **IO**, **filesystem/path concerns**, and **where different kinds of data should be stored** for both:

- a **game** built with Gecko, and
- the **Gecko Editor** (which is itself built using the same engine).

The goal is to keep the system **flexible for game developers** while respecting Gecko’s layering rules and avoiding initialization-order traps.

---

## 1) Core Services: why IO should NOT be a Core service

Gecko Core has a strict service installation order and an important constraint:

> Services shouldn’t rely on other services before being initialized (or during initialization).

The dependency ladder in Core is intentionally simple:

- **Level 0:** Allocator (no dependencies)
- **Level 1:** JobSystem (may use allocator)
- **Level 2:** Profiler (may use allocator and job system)
- **Level 3:** Logger (may use allocator, job system, and profiler)

If you introduce IO as a service, you quickly run into cycles like:

- IO init wants to report errors → uses Logger → but Logger isn’t initialized yet.
- Logger uses file sinks → sinks need IO → IO might not be installed yet.

These are solvable, but they add complexity and “gotchas” to the boot process.

### Design conclusion

Treat filesystem/IO as an **OS abstraction** (Platform), not a Core service.

This keeps Core:

- portable
- deterministic at startup
- free of OS conventions (XDG/AppData/etc.)

---

## 2) Recommended layering

### Core

- foundational interfaces (allocator/jobs/log/profiler)
- no OS-specific path rules
- no “known folder” resolution

### Platform

Owns OS abstraction (implemented per-platform):

- filesystem primitives (open/read/write/close)
- “known folders” (user config/data/cache/temp)
- executable location, environment variables

Importantly: platform APIs should be usable without relying on Core services.

### Runtime

Owns concrete implementations and “batteries included” features:

- log sinks / profiler sinks (console/file/trace)
- asset tooling/importers (future)
- editor-facing features (future)

Runtime may use Platform for actual IO.

---

## 3) “Data” isn’t one thing: model it as domains

Trying to pick a single directory strategy early ("everything in AppData") tends to fail because different kinds of data have different expectations.

A small set of **domains** scales well across:

- game vs editor
- per-user vs per-project
- persistent vs disposable
- Linux vs Windows vs macOS

Suggested domains:

| Domain | Examples | Lifetime | Typical intent |
|---|---|---|---|
| Install (read-only) | shipped assets, default config | tied to installation | immutable at runtime |
| Project | `.gecko/`, import DB, generated files | per-project | workspace-local state |
| UserConfig | settings, keybinds, editor prefs | persistent | user-specific configuration |
| UserData | saves, profiles, user content | persistent | user-specific data |
| Cache | shader cache, thumbnails, derived data | disposable | safe to delete |
| Temp | transient scratch | disposable | short-lived |

### Why domains help

- The **Editor** and a **Game** can use the same API.
- Only the policy (identity + project root + overrides) changes.
- You can support **portable mode** easily (store user config/data next to the executable).

---

## 4) Put “path resolution” in Platform

Platform should expose OS-aware path helpers such as:

- `GetUserConfigDir()`
- `GetUserDataDir()`
- `GetCacheDir()`
- `GetTempDir()`
- `GetExecutableDir()`

On Linux, this usually follows XDG:

- config: `$XDG_CONFIG_HOME` (fallback `~/.config`)
- data: `$XDG_DATA_HOME` (fallback `~/.local/share`)
- cache: `$XDG_CACHE_HOME` (fallback `~/.cache`)

Windows/macOS equivalents belong in Platform as well.

**Guideline:** avoid logging from these functions (or keep it optional) to prevent dependency loops.

---

## 5) Put “policy” in Runtime (or a thin app layer)

Platform answers: “what is the correct OS folder for config/data/cache?”

Runtime/app policy answers:

- What is the **app identity**? (organization + product)
- Are we the **Editor** or a **Game**?
- What is the **project root** (if any)?
- Are there **overrides**? (portable mode, CLI flags, env vars)

This keeps things developer-friendly and customizable.

---

## 6) A practical interface shape

A good mental model:

1. Choose a **domain**.
2. Provide an **identity**.
3. Resolve a domain root (platform + policy).
4. Append a **relative path**.

Example use cases:

- Game settings:
  - domain: `UserConfig`
  - relative: `settings.toml`
- Game save:
  - domain: `UserData`
  - relative: `saves/slot1.json`
- Editor preferences:
  - domain: `UserConfig`
  - relative: `editor/prefs.json`
- Editor import DB (project-local):
  - domain: `Project`
  - relative: `.gecko/import-db.bin`
- Shader cache:
  - domain: `Cache`
  - relative: `shaders/vulkan/cache.bin`

Optional but useful knobs:

- **Portable mode**: force config/data/cache under `Install` or `ExecutableDir`.
- **Project-derived data**: choose whether derived artifacts go under `Project` or `Cache`.

---

## 7) Assets: domain paths vs mounts (future-friendly)

Assets commonly come from multiple places:

- shipped assets: install/read-only
- project assets: project root (editor)
- mods: user data (optional)

Domains solve “where do I put files?” but assets often want a higher-level system later:

- mount points like `assets:/`, `config:/`, `cache:/`
- search path precedence
- pak/zip support

That can be layered on top of Platform path APIs without changing Core services.

---

## 8) Guardrails (easy-to-remember rules)

- Keep Core service boot deterministic; don’t pull IO into `core::Services`.
- Put OS conventions in Platform (XDG/AppData/macOS folders).
- Put identity + overrides in Runtime/app policy.
- Treat data as domains (config/data/cache/project), not “a folder”.
- Avoid init-time cycles (IO ↔ Logger) by keeping path functions independent.
