#pragma once

#include "gecko/core/types.h"
#include "gecko/math/rect.h"

#include <cstring>

namespace gecko::platform {

static constexpr u32 MaxMonitorNameLength = 128;

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
  Srgb,
  Hdr10,
  DolbyVision,
};

struct MonitorInfo
{
  char Name[MaxMonitorNameLength] {};
  math::Rect2D Bounds {};
  math::Rect2D WorkArea {};
  u32 RefreshRateMilliHz {60000};
  u32 Dpi {96};
  float DpiScale {1.0F};
  ColorSpace ColorSpace {ColorSpace::Srgb};
  bool IsPrimary {false};

  void SetName(const char* name) noexcept
  {
    if (name)
    {
      const auto len = ::std::strlen(name);
      const auto count =
          len < MaxMonitorNameLength - 1 ? len : MaxMonitorNameLength - 1;
      ::std::memcpy(Name, name, count);
      Name[count] = '\0';
    }
  }
};

struct MonitorBounds
{
  math::Rect2D Bounds {};
  math::Rect2D WorkArea {};
};

}  // namespace gecko::platform
