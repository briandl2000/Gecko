#pragma once

/// @file
/// Assertion and verification macros.
///
/// - `GECKO_ASSERT(x)` -- debug-only check; compiled to a no-op when
///   `NDEBUG` is defined. Use for invariants that must hold at runtime
///   but are too costly or noisy to validate in release.
/// - `GECKO_VERIFY(x)` -- evaluates `x` in every build configuration.
///   In debug builds a failed condition fires `GECKO_ASSERT`; in
///   release builds the side effects of `x` still happen but the
///   result is discarded. Use when the expression has side effects you
///   need in release.

#include <cassert>

#ifndef GECKO_ASSERT
#if defined(NDEBUG)
#define GECKO_ASSERT(x) ((void)0)
#else
#define GECKO_ASSERT(x) assert(x)
#endif
#endif

#ifndef GECKO_VERIFY
#define GECKO_VERIFY(x)          \
  do                             \
  {                              \
    if (!(x))                    \
    {                            \
      GECKO_ASSERT(false && #x); \
    }                            \
  } while (0)
#endif
