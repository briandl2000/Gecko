#pragma once

/// @file
/// Compile-time accessors for the Gecko engine version.
///
/// All accessors are `constexpr` and resolve to the macros defined in
/// checked-in `gecko/version.h`. Use `VersionFullString()` for
/// human-readable banners (includes prerelease tag).

#include "gecko/core/api.h"
#include "gecko/version.h"

namespace gecko {

inline constexpr u32 EngineAbiVersion = 1;

[[nodiscard]] constexpr u32 PackVersion(u32 major, u32 minor, u32 patch) noexcept
{
  return (major << 24U) | (minor << 16U) | patch;
}

/// @return Major version component (`X` in `X.Y.Z`).
[[nodiscard]]
inline constexpr int VersionMajor() noexcept
{
  return GECKO_VERSION_MAJOR;
}

/// @return Minor version component (`Y` in `X.Y.Z`).
[[nodiscard]]
inline constexpr int VersionMinor() noexcept
{
  return GECKO_VERSION_MINOR;
}

/// @return Patch version component (`Z` in `X.Y.Z`).
[[nodiscard]]
inline constexpr int VersionPatch() noexcept
{
  return GECKO_VERSION_PATCH;
}

/// @return `"X.Y.Z"` formatted version string, no prerelease tag.
[[nodiscard]]
inline constexpr const char* VersionString() noexcept
{
  return GECKO_VERSION_STRING;
}

/// @return Prerelease tag (e.g. `"alpha.3"`) or `""` for stable builds.
[[nodiscard]]
inline constexpr const char* VersionPrerelease() noexcept
{
  return GECKO_VERSION_PRERELEASE;
}

/// @return Full version string including prerelease tag, suitable for
///         logging and banner output.
[[nodiscard]]
inline constexpr const char* VersionFullString() noexcept
{
  return GECKO_VERSION_FULL_STRING;
}

[[nodiscard]] constexpr u32 VersionPacked() noexcept
{
  return PackVersion(GECKO_VERSION_MAJOR, GECKO_VERSION_MINOR, GECKO_VERSION_PATCH);
}

}  // namespace gecko
