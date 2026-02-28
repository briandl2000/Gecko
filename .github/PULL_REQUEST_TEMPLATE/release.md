<!-- This template is for release PRs: dev → main only.
     Create via: https://github.com/briandl2000/Gecko/compare/main...dev -->

## Release to Main

**Version:** `x.y.z-pre` → `x.y.z-pre`

### Summary
<!-- Brief description of what this release includes -->


### Pre-merge Checklist
- [ ] **Base: `main` ← Head: `dev`** (confirm branches are correct)
- [ ] Version bumped in `CMakeLists.txt` (`project(Gecko VERSION x.y.z)` and/or `GECKO_VERSION_PRERELEASE`)
- [ ] `CHANGELOG.md` updated (moved entries from `[Unreleased]` to `[x.y.z]` section)
- [ ] All CI checks pass (build, test, version check, changelog check)
- [ ] No known regressions from dev testing

### Merge Instructions
- Use **Create a merge commit** (not squash or rebase) to preserve dev history
- After merge, CI will automatically tag and create a GitHub release

### Release Notes Preview
<!-- Copy the CHANGELOG.md section for this version here for review -->

