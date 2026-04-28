#pragma once

/// @file
/// Hashed string identifier used by the logger, profiler, and event bus.
///
/// A `Label` pairs a 64-bit FNV-1a hash of the source string with the
/// original C-string pointer. Comparison is hash-only; the name pointer
/// exists for human-readable formatting in sinks. Construct labels at
/// compile time with `MakeLabel("dotted.path")` so the hash is folded
/// at the call site.

#include "gecko/core/types.h"
#include "gecko/core/utility/hash.h"

namespace gecko {

/// Hashed string identifier (64-bit FNV-1a).
///
/// `Id == 0` denotes an empty/invalid label; any non-empty source
/// string is guaranteed to hash to a non-zero value by `MakeLabel`.
struct Label
{
  u64 Id {0};
  const char* Name {nullptr};

  /// @return `true` for any label produced from a non-empty string.
  constexpr bool IsValid() const noexcept
  {
    return Id != 0;
  }

  constexpr bool operator==(const Label& other) const noexcept
  {
    return Id == other.Id;
  }
  constexpr bool operator!=(const Label& other) const noexcept
  {
    return Id != other.Id;
  }
};

/// Build a `Label` from a C-string at compile time.
///
/// Returns an empty label (`Id == 0`) for `nullptr` or `""`.
///
/// @param fullName  Dotted path identifying the label
///                  (e.g. `"gecko.runtime.logger"`).
constexpr Label MakeLabel(const char* fullName) noexcept
{
  if (!fullName || *fullName == '\0')
  {
    return {};
  }
  return Label {FNV1a64(fullName), fullName};
}
}  // namespace gecko
