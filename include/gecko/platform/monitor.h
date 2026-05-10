#pragma once

/// @file
/// Monitor descriptor types used by the monitor query service.
///
/// `MonitorHandle` is an opaque non-owning identifier; `MonitorInfo`
/// carries a snapshot of the monitor's geometry, refresh rate and DPI.
/// Listing monitors and resolving handles to info is done via
/// `IMonitorsBackend` (see `gecko/platform/monitors_interface.h`).

#include "gecko/core/types.h"
#include "gecko/math/rect.h"

#include <cstring>

namespace gecko::platform {

/// Maximum number of bytes (including NUL) reserved for `MonitorInfo::Name`.
static constexpr u32 MaxMonitorNameLength = 128;

/// Opaque identifier for a monitor reported by the platform backend.
struct MonitorHandle
{
  u64 Id {0};  ///< Backend-specific identifier; `0` means invalid.

  MonitorHandle() = default;
  explicit MonitorHandle(u64 id) noexcept : Id(id)
  {}

  /// `true` when the handle refers to a (formerly) live monitor.
  bool IsValid() const noexcept
  {
    return Id != 0;
  }
  /// Mark the handle as invalid; does not affect the underlying monitor.
  void Reset() noexcept
  {
    Id = 0;
  }

  bool operator==(const MonitorHandle& other) const noexcept
  {
    return Id == other.Id;
  }
  bool operator!=(const MonitorHandle& other) const noexcept
  {
    return Id != other.Id;
  }
};

/// Color space reported by a monitor (best-effort; backend-dependent).
enum class ColorSpace : u8
{
  Unknown,      ///< Backend could not determine the color space.
  Srgb,         ///< Standard sRGB.
  Hdr10,        ///< HDR10 (BT.2020 + PQ).
  DolbyVision,  ///< Dolby Vision.
};

/// Snapshot of a monitor's properties at the time of query.
struct MonitorInfo
{
  char Name[MaxMonitorNameLength] {};               ///< NUL-terminated UTF-8 name.
  math::Rect2D Bounds {};                           ///< Full monitor bounds in virtual desktop pixels.
  math::Rect2D WorkArea {};                         ///< Bounds excluding OS taskbars/docks.
  u32 RefreshRateMilliHz {60000};                   ///< Refresh rate in millihertz (60 Hz = `60000`).
  u32 Dpi {96};                                     ///< Logical DPI.
  float DpiScale {1.0F};                            ///< Linear scale factor (`Dpi / 96`).
  ColorSpace MonitorColorSpace {ColorSpace::Srgb};  ///< Reported color space.
  bool IsPrimary {false};                           ///< `true` for the system primary monitor.

  /// Copy `name` into `Name`, truncating to `MaxMonitorNameLength - 1`
  /// bytes and always NUL-terminating. No-op when `name` is null.
  void SetName(const char* name) noexcept
  {
    if (name)
    {
      const auto len = ::std::strlen(name);
      const auto count = len < MaxMonitorNameLength - 1 ? len : MaxMonitorNameLength - 1;
      ::std::memcpy(Name, name, count);
      Name[count] = '\0';
    }
  }
};

/// Lightweight pair of `Bounds` + `WorkArea` for callers that don't need
/// the rest of `MonitorInfo`.
struct MonitorBounds
{
  math::Rect2D Bounds {};    ///< Full monitor bounds.
  math::Rect2D WorkArea {};  ///< Bounds excluding OS taskbars/docks.
};

}  // namespace gecko::platform
