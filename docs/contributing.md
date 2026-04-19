# Contributing / Workflow

This is a hobby project with real structure. The goal is to keep changes easy to understand and hard to break.

## Branching Model

```
feature/xyz ──PR──> dev ──release PR──> main
                     │                    │
              (prerelease builds)   (stable releases)
```

- **`main`** — Stable releases only. Always builds, always tagged. Never commit directly.
- **`dev`** — Integration branch. All feature work merges here first. Pushes trigger prerelease builds.
- **`feature/<name>`** or **`fix/<name>`** — Short-lived branches off `dev` for individual changes.

### PR Rules

| PR type | Base branch | Template | CI checks |
|---------|------------|----------|-----------|
| Feature / bugfix | `dev` | Default template | build-and-test |
| Release | `main` | [release template](../.github/PULL_REQUEST_TEMPLATE/release.md) | build-and-test + pr-checks (version bump, changelog) |

> **Important:** When creating a PR, GitHub defaults the base to `main`. **Change it to `dev`** for feature/fix PRs.

### Release Process (dev → main)

1. Ensure all work is merged to `dev` and CI is green
2. Bump version in `CMakeLists.txt` (`GECKO_VERSION_PRERELEASE` or `VERSION`)
   - `version.h` is auto-generated from `version.h.in` at configure time — no manual edits needed
3. Update `CHANGELOG.md` — move `[Unreleased]` entries to a new version section
4. Commit & push to `dev`
5. Create PR: **dev → main** using the [release template](https://github.com/briandl2000/Gecko/compare/main...dev?template=release.md)
6. Wait for all CI checks to pass (build-and-test + pr-checks)
7. Merge with **Create a merge commit** (preserves dev history)
8. CI automatically tags the release and creates GitHub release with packages

## Version control habits

- Prefer small, focused commits.
- Commit messages should explain **why** (not just what).
- Use short-lived feature branches when work spans multiple commits.

## Change size rule

If a change feels scary to review, it's too big.

- Split by subsystem (`platform` vs `runtime`) or by capability ("window" vs "input").
- Prefer adding a minimal API and one implementation path before generalizing.

## PR / review checklist (even if you're solo)

- Builds Debug + Release on both Linux and Windows
- Run at least one example affected by the change
- New public APIs are documented (docs hub + any relevant guide)
- No accidental module boundary leaks (Core should not depend on Platform)

## Using AI responsibly

- Use AI to learn APIs or explore alternatives.
- You implement the final code and can explain it.
- If AI produced code, rewrite/clean it so it matches Gecko style and architecture.
