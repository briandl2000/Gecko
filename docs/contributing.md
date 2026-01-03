# Contributing / Workflow

This is a hobby project with real structure. The goal is to keep changes easy to understand and hard to break.

## Version control habits

- Prefer small, focused commits.
- Commit messages should explain **why** (not just what).
- Use short-lived feature branches when work spans multiple commits.

Suggested branch style:
- `main` (always builds)
- `feature/<short-name>` (merge when stable)

## Change size rule

If a change feels scary to review, it’s too big.

- Split by subsystem (`platform` vs `runtime`) or by capability ("window" vs "input").
- Prefer adding a minimal API and one implementation path before generalizing.

## PR / review checklist (even if you’re solo)

- Builds Debug + Release
- Run at least one example affected by the change
- New public APIs are documented (docs hub + any relevant guide)
- No accidental module boundary leaks (Core should not depend on Platform)

## Using AI responsibly

- Use AI to learn APIs or explore alternatives.
- You implement the final code and can explain it.
- If AI produced code, rewrite/clean it so it matches Gecko style and architecture.
