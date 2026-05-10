#pragma once

/// @file
/// Public window value types.
///
/// `WindowHandle` is an opaque non-owning identifier; the windowing
/// service (`IWindows`) owns the actual platform window. `WindowDesc`
/// is the input to `IWindows::Create`; the rest of the types describe
/// presentation modes and DPI metadata.

#include "gecko/core/types.h"
#include "gecko/core/utility/bit.h"
#include "gecko/math/vector.h"
#include "gecko/platform/platform_config.h"

namespace gecko::platform {

/// Width/height pair, in pixels (unsigned).
struct Extent2D
{
  u32 Width {0};   ///< Width in pixels.
  u32 Height {0};  ///< Height in pixels.
};

/// Opaque identifier for a window owned by `IWindows`.
/// Default-constructed handles compare equal to `InvalidWindowHandle`.
struct WindowHandle
{
  static constexpr u64 InvalidId = 0;  ///< Sentinel value for invalid handles.

  u64 Id {InvalidId};  ///< Backend-specific identifier; `0` means invalid.

  WindowHandle() = default;
  explicit WindowHandle(u64 id) noexcept : Id(id)
  {}

  /// `true` when the handle refers to (or once referred to) a window.
  bool IsValid() const noexcept
  {
    return Id != InvalidId;
  }
  /// Make the handle invalid (without closing the underlying window).
  void Reset() noexcept
  {
    Id = InvalidId;
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

/// Presentation mode for a window.
enum class WindowMode : u8
{
  Windowed,              ///< Movable, decorated window.
  Fullscreen,            ///< Exclusive fullscreen on the current monitor.
  BorderlessFullscreen,  ///< Borderless window covering the current monitor.
};

/// Cursor capture/visibility behaviour for a focused window.
enum class CursorMode : u8
{
  Normal,  ///< Visible, free movement.
  Hidden,  ///< Invisible, free movement.
  Locked,  ///< Captured: cursor is hidden and constrained to the window.
};

/// High-level visibility / minimize state.
enum class WindowState : u8
{
  Normal,     ///< Visible at its requested size.
  Minimized,  ///< Iconified / minimized.
  Maximized,  ///< Maximized to the work area of its monitor.
  Hidden,     ///< Mapped but invisible.
};

/// Bitmask describing which titlebar buttons should be visible.
enum class WindowButtons : u8
{
  None = 0,
  Close = Bit(0),     ///< Show the close button.
  Minimize = Bit(1),  ///< Show the minimize button.
  Maximize = Bit(2),  ///< Show the maximize button.
  All = Close | Minimize | Maximize,
};

/// DPI / scale information for a monitor or window.
struct DpiInfo
{
  u32 Dpi {96};        ///< Logical DPI; 96 is "100%" on Windows.
  float Scale {1.0F};  ///< Linear scale factor (`Dpi / 96`).
};

/// Description of a window to be created via `IWindows::Create`.
struct WindowDesc
{
  const char* Title {"Gecko"};                 ///< UTF-8 title.
  math::Int2 Size {1280, 720};                 ///< Initial client size in pixels.
  WindowMode Mode {WindowMode::Windowed};      ///< Initial presentation mode.
  WindowButtons Buttons {WindowButtons::All};  ///< Visible titlebar buttons.
  bool Resizable {true};                       ///< User may resize the frame.
  bool Visible {true};                         ///< Show the window on creation.
  bool Decorated {true};                       ///< Draw native chrome (title, border).
  bool HighDpi {true};                         ///< Opt into per-monitor DPI awareness.
};

/// Opaque native window pointers, useful for handing off to a graphics
/// backend (Vulkan, OpenGL, etc.). The meaning of `Handle` and
/// `Display` depends on `Backend`.
struct NativeWindowHandle
{
  DisplayBackendKind Backend {DisplayBackendKind::Unknown};  ///< Active backend.
  void* Handle {nullptr};                                    ///< Native window handle (HWND, xcb_window_t,
                                                             ///< wl_surface*, ...).
  void* Display {nullptr};                                   ///< Native display/connection (X Display*,
                                                             ///< wl_display*, nullptr on Win32).
};

}  // namespace gecko::platform
