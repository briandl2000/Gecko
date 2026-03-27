#pragma once

#include "gecko/core/types.h"
#include "gecko/math/rect.h"

namespace gecko::platform {

struct MonitorHandle
{
  u64 Id {0};

  MonitorHandle() = default;
  explicit MonitorHandle(u64 id) noexcept : Id(id)
  {}

  bool IsValid() const noexcept
  {
    return Id != 0;
  }
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

enum class ColorSpace : u8
{
  Unknown,
  SRGB,
  HDR10,
  DolbyVision,
};

struct MonitorInfo
{
  math::Rect2D Bounds {};
  math::Rect2D WorkArea {};
  u32 RefreshRateMilliHz {60000};
  u32 Dpi {96};
  float DpiScale {1.0F};
  ColorSpace ColorSpace {ColorSpace::SRGB};
  bool IsPrimary {false};
};

}  // namespace gecko::platform
