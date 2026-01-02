#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko::platform {

// Identifies (a) the selected backend or (b) the native window system used.
// `Auto` and `Null` are selection-only.
enum class WindowBackendKind : u8 {
  Unknown,
  Auto,
  Null,

  Win32,
  Xlib,
  Xcb,
  Wayland,
  Cocoa,
};

struct WindowHandle {
  u64 Id{0};

  WindowHandle() = default;
  explicit WindowHandle(u64 id) noexcept : Id(id) {}

  bool IsValid() const noexcept { return Id != 0; }
  void Reset() noexcept { Id = 0; }

  bool operator==(const WindowHandle &other) const noexcept {
    return Id == other.Id;
  }
  bool operator!=(const WindowHandle &other) const noexcept {
    return Id != other.Id;
  }
};

struct MonitorHandle {
  u64 Id{0};

  MonitorHandle() = default;
  explicit MonitorHandle(u64 id) noexcept : Id(id) {}

  bool IsValid() const noexcept { return Id != 0; }
  void Reset() noexcept { Id = 0; }

  bool operator==(const MonitorHandle &other) const noexcept {
    return Id == other.Id;
  }
  bool operator!=(const MonitorHandle &other) const noexcept {
    return Id != other.Id;
  }
};

struct Extent2D {
  u32 Width{0};
  u32 Height{0};
};

enum class WindowMode : u8 {
  Windowed,
  Fullscreen,
  BorderlessFullscreen,
};

enum class CursorMode : u8 {
  Normal,
  Hidden,
  Locked,
};

struct DpiInfo {
  u32 Dpi{96};
  float Scale{1.0F};
};

struct WindowDesc {
  const char *Title{"Gecko"};
  Extent2D Size{1280, 720};
  WindowMode Mode{WindowMode::Windowed};
  bool Resizable{true};
  bool Visible{true};
  bool HighDpi{true};
};

struct NativeWindowHandle {
  WindowBackendKind Backend{WindowBackendKind::Unknown};
  void *Handle{nullptr};
  void *Display{nullptr};
};

enum class WindowEventKind : u8 {
  None,
  CloseRequested,
  Closed,
  Resized,
  DpiChanged,
  Key,
  Char,
  MouseMove,
  MouseButton,
  MouseWheel,
};

struct WindowEvent {
  WindowEventKind Kind{WindowEventKind::None};
  WindowHandle Window{};
  u64 TimeNs{0};

  union {
    struct {
      u32 Width;
      u32 Height;
    } Resize;

    struct {
      u32 Dpi;
      float Scale;
    } Dpi;

    struct {
      u32 Key;
      u8 Down;
      u8 Repeat;
    } Key;

    struct {
      u32 Codepoint;
    } Char;

    struct {
      i32 X;
      i32 Y;
    } MouseMove;

    struct {
      u8 Button;
      u8 Down;
    } MouseButton;

    struct {
      float DeltaX;
      float DeltaY;
    } MouseWheel;
  } Data{};
};

} // namespace gecko::platform
