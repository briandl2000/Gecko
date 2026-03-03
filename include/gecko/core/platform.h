#pragma once

// ─── OS detection ────────────────────────────────────────────────────────────
// These are set from compiler-predefined macros so they are always available
// without requiring a CMake configure step.  All other code should use these
// GECKO_PLATFORM_* macros instead of the raw compiler builtins.

#if defined(_WIN32) && !defined(GECKO_PLATFORM_WINDOWS)
#define GECKO_PLATFORM_WINDOWS 1
#endif

#if defined(__linux__) && !defined(GECKO_PLATFORM_LINUX)
#define GECKO_PLATFORM_LINUX 1
#endif

#if defined(__APPLE__) && !defined(GECKO_PLATFORM_APPLE)
#define GECKO_PLATFORM_APPLE 1
#endif

// ─── Display-server feature macros (set by CMake at configure time) ──────────
// GECKO_PLATFORM_LINUX_X11      – X11/Xlib backend was found and enabled.
// GECKO_PLATFORM_LINUX_WAYLAND  – Wayland backend was found and enabled.
// These are compile-definitions injected by src/platform/CMakeLists.txt and
// are only relevant when GECKO_PLATFORM_LINUX is defined.
