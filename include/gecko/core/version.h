#pragma once

#include "gecko/core/api.h"

namespace gecko {

[[nodiscard]]
GECKO_API inline constexpr int VersionMajor() noexcept
{
  return GECKO_VERSION_MAJOR;
}

[[nodiscard]]
GECKO_API inline constexpr int VersionMinor() noexcept
{
  return GECKO_VERSION_MINOR;
}

[[nodiscard]]
GECKO_API inline constexpr int VersionPatch() noexcept
{
  return GECKO_VERSION_PATCH;
}

[[nodiscard]]
GECKO_API inline constexpr const char* VersionString() noexcept
{
  return GECKO_VERSION_STRING;
}

[[nodiscard]]
GECKO_API inline constexpr const char* VersionPrerelease() noexcept
{
  return GECKO_VERSION_PRERELEASE;
}

[[nodiscard]]
GECKO_API inline constexpr const char* VersionFullString() noexcept
{
  return GECKO_VERSION_FULL_STRING;
}

}  // namespace gecko
