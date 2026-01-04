# Release Workflow

This document defines how Gecko branches, versions, and ships prebuilt SDK artifacts.

## Branch model

- `main`: latest **stable** engine (always builds).
- `develop`: integration branch for the next release.
- `feature/<name>`: short-lived branches off `develop`.

Optional (only when needed):
- `release/<version>`: stabilization branch (bugfix-only).
- `hotfix/<version>`: urgent fix off `main`.

## Versioning

Gecko uses SemVer-like versions with prerelease identifiers.

- Tags: `v0.0.0-alpha.0`, `v0.0.0-alpha.1`, … then `v0.0.0-beta.0`, …
- A release is defined by an **annotated git tag** on `main`.

Practical rule:
- Every tagged prerelease must build and install from a clean checkout.

### Where version lives

The CMake project version is set in `CMakeLists.txt` and currently supports an optional prerelease string.

Policy:
- For each prerelease tag, set `GECKO_VERSION_PRERELEASE` accordingly (e.g. `alpha.0`).
- Keep `PROJECT_VERSION` at `0.0.0` until you decide you want to bump major/minor/patch.

## Development workflow (repeatable)

1) Pick a ticket from the target milestone (e.g. `v0.0.0-alpha.0`).
2) Create feature branch from `develop`:
   - `git checkout develop`
   - `git pull`
   - `git checkout -b feature/<name>`
3) Implement in small commits.
4) Verify locally:
   - `cmake --preset debug && cmake --build build --config Debug`
   - run an example affected by the change.
5) Merge into `develop` (fast-forward or merge commit is fine; be consistent).
6) When milestone scope is complete, cut a stabilization branch *only if needed*:
   - `git checkout develop && git checkout -b release/v0.0.0-alpha.0`
   - allow bugfix-only changes
7) Finalize release:
   - merge `release/...` into `main`
   - tag on `main`

## When to merge to `main`

- Merge to `main` only when you intend to ship (or when you are confident it’s “stable enough” for outsiders).
- Everything else lands in `develop`.

## Packaging: what to ship

Ship an SDK layout that supports both “download zip” and “submodule + build” users.

**Install tree (recommended)**

- `include/` 
  - `include/gecko/**` public headers
- `lib/`
  - compiled libraries
- `lib/cmake/Gecko/`
  - `GeckoConfig.cmake`, `GeckoConfigVersion.cmake`, `GeckoTargets.cmake`
- `LICENSE`

This is already aligned with `cmake/install.cmake` + exported targets.

## CI release pipeline (GitHub Actions outline)

Trigger on tags matching `v*`.

Per platform job (matrix):
1) Checkout
2) Configure (Release preset)
3) Build
4) Install to a staging prefix (e.g. `staging/Gecko`)
5) Package:
   - Linux: `tar.gz` and/or `.zip`
   - Windows: `.zip`
6) Upload artifact
7) Create/attach to GitHub Release for the tag

## Release checklist (human)

Before tagging:
- `develop` builds Debug + Release.
- Examples run on at least one platform.
- `cmake --install` produces complete SDK tree.
- Public headers compile for a simple external consumer (smoke test).
- Changelog/notes are written (even a short bullet list).

Tagging steps:
- Merge to `main`
- Set prerelease string
- Tag: `git tag -a v0.0.0-alpha.0 -m "v0.0.0-alpha.0"`
- Push: `git push origin main --tags`
