# Changelog

All notable changes to Gecko will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.0.0-alpha.1]

### Added
- Automated CI releases for both dev and main branches
- PR templates (feature and release) with checklists
- PR checks: version bump and changelog enforcement for main merges
- Consistent version strings across Linux/Windows packages
- CHANGELOG.md for tracking changes

### Fixed
- CI version extraction failing on Windows (`grep -oP` replaced with portable `sed`)
- Linux/Windows packages having mismatched timestamps

## [0.0.0-alpha.0]

### Added
- Core module: services, memory management, scope system, boot sequence
- Math module: vectors, matrices, quaternions, AABBs
- Platform module: windowing (X11 on Linux, Win32 on Windows)
- Runtime module: logging, profiling, job system, module registry, event bus
- CI pipeline with automated packaging and GitHub releases
- Cross-platform support (Linux + Windows) with Clang

### Changed
- Replaced `grep -oP` with portable `sed` in CI for Windows compatibility

### Fixed
- CI version extraction failing on Windows due to `grep -P` not supporting locale
