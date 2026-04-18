#pragma once

#include "gecko/core/types.h"
#include "gecko/core/utility/bit.h"
#include "gecko/math/vector.h"
#include "gecko/platform/platform_config.h"

namespace gecko::platform {

struct Extent2D
{
  u32 Width {0};
  u32 Height {0};
};

struct WindowHandle
{
  u64 Id {0};

  WindowHandle() = default;
  explicit WindowHandle(u64 id) noexcept : Id(id)
  {}

  bool IsValid() const noexcept
  {
    return Id != 0;
  }
  void Reset() noexcept
  {
    Id = 0;
  }

  bool operator==(const WindowHandle& other) const noexcept
  {
    return Id == other.Id;
  }
  bool operator!=(const WindowHandle& other) const noexcept
  {
    return Id != other.Id;
  }
};

enum class WindowMode : u8
{
  Windowed,
  Fullscreen,
  BorderlessFullscreen,
};

enum class CursorMode : u8
{
  Normal,
  Hidden,
  Locked,
};

enum class WindowState : u8
{
  Normal,
  Minimized,
  Maximized,
  Hidden,
};

enum class WindowButtons : u8
{
  None = 0,
  Close = Bit(0),
  Minimize = Bit(1),
  Maximize = Bit(2),
  All = Close | Minimize | Maximize,
};

struct DpiInfo
{
  u32 Dpi {96};
  float Scale {1.0F};
};

struct WindowDesc
{
  const char* Title {"Gecko"};
  math::Int2 Size {1280, 720};
  WindowMode Mode {WindowMode::Windowed};
  WindowButtons Buttons {WindowButtons::All};
  bool Resizable {true};
  bool Visible {true};
  bool Decorated {true};
  bool HighDpi {true};
};

struct NativeWindowHandle
{
  DisplayBackendKind Backend {DisplayBackendKind::Unknown};
  void* Handle {nullptr};
  void* Display {nullptr};
};

}  // namespace gecko::platform
