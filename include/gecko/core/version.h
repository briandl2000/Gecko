#pragma once

/// @file
/// Compile-time accessors for the Gecko engine version.
///
/// All accessors are `constexpr` and resolve to the macros defined in
/// `gecko/version.h` at configure time. Use `VersionFullString()` for
/// human-readable banners (includes prerelease tag).

#include "gecko/core/api.h"
#include "gecko/version.h"

namespace gecko {

/// @return Major version component (`X` in `X.Y.Z`).
[[nodiscard]]
GECKO_API inline constexpr int VersionMajor() noexcept
{
  return GECKO_VERSION_MAJOR;
}

/// @return Minor version component (`Y` in `X.Y.Z`).
[[nodiscard]]
GECKO_API inline constexpr int VersionMinor() noexcept
{
  return GECKO_VERSION_MINOR;
}

/// @return Patch version component (`Z` in `X.Y.Z`).
[[nodiscard]]
GECKO_API inline constexpr int VersionPatch() noexcept
{
  return GECKO_VERSION_PATCH;
}

/// @return `"X.Y.Z"` formatted version string, no prerelease tag.
[[nodiscard]]
GECKO_API inline constexpr const char* VersionString() noexcept
{
  return GECKO_VERSION_STRING;
}

/// @return Prerelease tag (e.g. `"alpha.3"`) or `""` for stable builds.
[[nodiscard]]
GECKO_API inline constexpr const char* VersionPrerelease() noexcept
{
  return GECKO_VERSION_PRERELEASE;
}

/// @return Full version string including prerelease tag, suitable for
///         logging and banner output.
[[nodiscard]]
GECKO_API inline constexpr const char* VersionFullString() noexcept
{
  return GECKO_VERSION_FULL_STRING;
}

}  // namespace gecko
