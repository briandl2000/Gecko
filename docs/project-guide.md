# Project Guide / Roadmap

This is the single place to answer:

- What should I work on next?
- How do I stay organized?
- How do I use AI without becoming dependent?

Gecko is a hobby project.
Goal: **good architecture** + **steady progress**.

---

## Quick: what to do next (easy to digest)

If reading is hard, just follow this checklist.

### Today (45–90 minutes)

- [ ] Pick ONE canary example to keep runnable (suggestion: `platform_example`)
- [ ] Create ONE ticket: “Platform MVP: windowing + event system + input + IO + monitors”
- [ ] Create ONE tiny ticket you can finish today
- [ ] Build + run the canary once

### This week (2–4 sessions)

- [ ] Work only from 1–3 tickets
- [ ] End each session with: build + run canary
- [ ] Write a 2–3 bullet note in the ticket: what changed + why

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

## 2.1) Step-by-step reset plan (to feel unblocked again)

Use this sequence any time the project starts to feel “too big”. It is intentionally mechanical.

### Step 1 — Make the docs boring (30–60 min)

- Treat [docs/README.md](README.md) as the only entry point.
- Keep only these canonical docs “in rotation”:
	- [docs/architecture.md](architecture.md)
	- [docs/build.md](build.md)
	- [docs/project-guide.md](project-guide.md)
	- [docs/contributing.md](contributing.md)
	- [docs/modules/README.md](modules/README.md)
- Everything else is either a reference or gets deleted/migrated.

### Step 2 — Pick *one* tracking tool (10 min)

Pick either:

- GitHub Issues (ties work to commits), or
- Trello (simple visual cards)

Then commit to using it for 2 months without switching.

### Step 3 — Create a “Now” list (20–30 min)

Create exactly **7 cards/issues**:

- 1 theme card: “Platform MVP: windowing + event system + input + IO + monitors”
- 3 implementation cards (small)
- 2 bug/cleanup cards (small)
- 1 docs card (small)

If you want help sizing: every card should be ~1 evening.

### Step 4 — Make a runnable target the center (15 min)

Choose one example to be your canary (suggestion: `platform_example`).

Rule: every week ends with “canary runs, even if feature incomplete”.

### Step 5 — Work in 45-minute blocks (ongoing)

For each session:

1. Pick 1 card.
2. Write a 3–5 bullet approach note (in the card).
3. Implement.
4. Build.
5. Run the canary.
6. Write a 2–3 bullet wrap-up note.

This prevents “lost weekends” and makes progress visible.

---

## 3) Planning + issue tracking (pick one system)

You need exactly one source of truth. Two good options:

### Option A: GitHub Issues + (optional) GitHub Projects

- Pros: ties directly to commits/PRs, searchable, good history
- Cons: a bit more “software project” feeling

Minimum labels:
- `core`, `platform`, `runtime`, `docs`, `tooling`
- `bug`, `feature`, `refactor`

Recommended issue template (keep it short):
- Problem / goal
- Approach (1–3 bullets)
- Done when (acceptance)

### Option B: Trello (keep it very small)

Lists:
- Backlog
- Next (max 10)
- Doing (max 3)
- Done

Rule: if a card is bigger than ~1–2 evenings, split it.

---

## 4) “Write it myself” guardrails (AI boundaries)

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

## 5) Roadmap (milestones)

## v0.0.0-alpha.0 release target (what “done” means)

This repo is in active development. For the first tagged alpha, treat this as the intended scope.

### In scope

- **Platform MVP**
	- windowing (create/destroy, resize at minimum)
	- decide on an event system + make the event pump reliable
	- input (keyboard/mouse: events + state snapshot)
	- platform IO primitives (filesystem/path conventions where they differ per OS)
	- monitor enumeration / primary monitor selection
- **Frame lifecycle (minimal)**
	- a stable “app skeleton” loop that demonstrates `PollEvents → Update → (Render stub) → Present stub → Sleep`
- **Packaging sanity**
	- `cmake --install` works and exported CMake targets are usable
- **Docs sanity**
	- build/run commands are accurate and there is one canary example kept runnable

### Out of scope (unless you explicitly pull it in)

- Graphics abstraction work
- Vulkan backend bring-up

### Release DoD (checklist)

- Builds Debug + Release
- Runs a canary example (suggestion: `platform_example`)
- `cmake --install` works for Release
- Public API changes reflected in docs (docs hub + this guide)

This roadmap is meant to be practical: always keep Gecko runnable.

### Milestone 0 — Foundations (mostly done)

Core services + runtime implementations (allocator/jobs/log/profiler) and examples.

### Milestone 1 — Platform MVP (windowing + events + input + IO + monitors)

Goal: an app can open a window, pump events, and read input reliably.

Suggested tickets:
- Harden window creation/destruction and event pump
- Decide on the event system (queue vs callback model, ownership, threading expectations)
- Define an input model (events + state snapshot)
- Handle resize + DPI changes (minimum: resize)
- Platform IO primitives (path conventions, basic file operations where needed)
- Monitor enumeration + primary monitor

### Milestone 2 — Frame lifecycle API (engine “heartbeat”)

Goal: a tiny contract for how an app runs each frame.

Suggested tickets:
- Define tick order: `PollEvents → Update → (Render stub) → Present stub → Sleep`
- Add a minimal “app skeleton” loop in examples

### Milestone 3 — Graphics abstraction (headers first)

Goal: define interfaces + lifetime rules *before* implementing backends.

Suggested tickets:
- Device/adapter selection API shape
- Swapchain interface tied to platform window handles
- Command queue/list model

### Milestone 4 — Vulkan backend bring-up

Goal: minimal present loop + debug defaults.

Suggested tickets:
- Instance + debug messenger
- Physical device selection + logical device
- Surface + swapchain (from platform)

### Milestone 5 — UI + ECS (later)

Only after the frame loop + graphics foundation are stable.

---

## 6) What to do next (recommended next 2–4 weeks)

If you want the least overwhelming, highest leverage path:

1. **Platform MVP**: window + event pump hardening
2. **Event system**: decide the model and wire it through the pump
3. **Input**: keyboard + mouse (events + state)
4. **Frame lifecycle**: a single example that runs a stable loop

These unlock everything else without committing to rendering decisions yet.

---

## 7) Motivation (practical, not inspirational)

If motivation dips, the fix is usually **smaller tasks** and **clearer finish lines**.

Use these rules:

- Always keep one “easy win” card in Doing.
- If you’re stuck for 15 minutes, stop and write down the unknown as a new card.
- Prefer finishing a tiny vertical slice over starting a big subsystem.

Two reliable ways to maintain momentum:

1. **Weekly demo to yourself** (2 minutes): run an example and point at the new behavior.
2. **Changelog habit** (1 minute): add a short note to the ticket or a small `notes` section in the card.

