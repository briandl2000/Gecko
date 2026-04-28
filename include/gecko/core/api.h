#pragma once

/// @file
/// Cross-platform DLL import/export macro.
///
/// `GECKO_API` resolves to `__declspec(dllexport)` when building Gecko
/// as a shared library on Windows, `__declspec(dllimport)` when
/// consuming it, and an empty token everywhere else (static builds and
/// non-Windows platforms). Annotate every public-API symbol that
/// crosses the module boundary with `GECKO_API`.

#if defined(GECKO_PLATFORM_WINDOWS) && GECKO_BUILD_SHARED
#ifdef GECKO_BUILDING
#define GECKO_API __declspec(dllexport)
#else
#define GECKO_API __declspec(dllimport)
#endif
#else
#define GECKO_API
#endif

/// Compiler-agnostic alias for the GCC/Clang `__PRETTY_FUNCTION__`
/// builtin. On MSVC this expands to `__FUNCSIG__`. Used by templates
/// that hash a unique per-type string at compile time
/// (e.g. `ServiceIdOf<T>()`).
#if defined(_MSC_VER) && !defined(__clang__)
#define GECKO_PRETTY_FUNCTION __FUNCSIG__
#else
#define GECKO_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif
