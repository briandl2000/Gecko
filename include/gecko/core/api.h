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
