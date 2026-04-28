#pragma once

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)

#include "gecko/core/services/events.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/windows_interface.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// Forward-declare X11 types to avoid macro conflicts (X11 defines "Always"
// which clashes with gecko enums).  The actual headers are included in the
// .cpp.
struct _XDisplay;
using Display = _XDisplay;
using Atom = unsigned long;
using Window = unsigned long;

namespace gecko::platform {

// -- Per-window state ---------------------------------------------------

struct X11WindowState
{
  WindowDesc Desc {};
  ::std::string TitleStorage;
  Extent2D ClientSize {};
  math::Int2 Position {0, 0};
  platform::WindowState State {platform::WindowState::Normal};
  CursorMode Cursor {CursorMode::Normal};
  WindowMode Mode {WindowMode::Windowed};
  WindowButtons Buttons {WindowButtons::All};
  Extent2D MinSize {0, 0};
  Extent2D MaxSize {0, 0};
  bool Decorated {true};
  bool Resizable {true};
  bool AlwaysOnTop {false};
  ::Window WindowId {0};
};

// -- Staged event -------------------------------------------------------

struct StagedEvent
{
  gecko::EventCode Code {0};
  u8 PayloadStorage[128] {};
  u32 PayloadSize {0};
};

template <typename T>
StagedEvent MakeStagedEvent(gecko::EventCode code, const T& payload) noexcept
{
  static_assert(sizeof(T) <= 128, "Payload too large for StagedEvent storage");
  StagedEvent ev;
  ev.Code = code;
  ev.PayloadSize = static_cast<u32>(sizeof(T));
  ::std::memcpy(ev.PayloadStorage, &payload, sizeof(T));
  return ev;
}

// -- MWM hints (Motif) --------------------------------------------------

struct MwmHints
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long inputMode;
  unsigned long status;
};

constexpr unsigned long MwmHintsDecorations = 1UL << 1;
constexpr unsigned long MwmHintsFunctions = 1UL << 0;
constexpr unsigned long MwmDecorAll = 1UL;
constexpr unsigned long MwmFuncResize = 1UL << 1;
constexpr unsigned long MwmFuncMove = 1UL << 2;
constexpr unsigned long MwmFuncMinimize = 1UL << 3;
constexpr unsigned long MwmFuncMaximize = 1UL << 4;
constexpr unsigned long MwmFuncClose = 1UL << 5;

// -- Backend class ------------------------------------------------------

class X11WindowsBackend final : public IWindowsBackend
{
public:
  X11WindowsBackend() noexcept;
  ~X11WindowsBackend() noexcept override;

  WindowHandle CreateWindow(const WindowDesc& desc) noexcept override;
  void DestroyWindow(WindowHandle window) noexcept override;
  bool IsWindowAlive(WindowHandle window) const noexcept override;
  bool RequestClose(WindowHandle window) noexcept override;

  Extent2D GetClientSize(WindowHandle window) const noexcept override;
  void SetClientSize(WindowHandle window, Extent2D size) noexcept override;
  void SetTitle(WindowHandle window, const char* title) noexcept override;
  const char* GetTitle(WindowHandle window) const noexcept override;
  void SetPosition(WindowHandle window, math::Int2 pos) noexcept override;
  math::Int2 GetPosition(WindowHandle window) const noexcept override;
  DpiInfo GetDpi(WindowHandle window) const noexcept override;
  NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept override;

  void SetWindowState(WindowHandle window,
                      platform::WindowState state) noexcept override;
  platform::WindowState GetWindowState(
      WindowHandle window) const noexcept override;
  void SetDecorated(WindowHandle window, bool decorated) noexcept override;
  bool IsDecorated(WindowHandle window) const noexcept override;
  void RequestFocus(WindowHandle window) noexcept override;

  void SetResizable(WindowHandle window, bool resizable) noexcept override;
  bool IsResizable(WindowHandle window) const noexcept override;
  void SetWindowMode(WindowHandle window, WindowMode mode) noexcept override;
  WindowMode GetWindowMode(WindowHandle window) const noexcept override;
  void SetWindowButtons(WindowHandle window,
                        WindowButtons buttons) noexcept override;
  WindowButtons GetWindowButtons(WindowHandle window) const noexcept override;
  void SetMinSize(WindowHandle window, Extent2D size) noexcept override;
  void SetMaxSize(WindowHandle window, Extent2D size) noexcept override;
  void SetAlwaysOnTop(WindowHandle window, bool topmost) noexcept override;
  bool IsAlwaysOnTop(WindowHandle window) const noexcept override;

  void SetCursorMode(WindowHandle window, CursorMode mode) noexcept override;
  CursorMode GetCursorMode(WindowHandle window) const noexcept override;

  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override;

private:
  void ApplyResizableHint(::Window w, const WindowDesc& desc) noexcept;
  void ApplyMotifDecorations(::Window w, bool enabled) noexcept;
  void ApplyMotifFunctions(::Window w, WindowButtons buttons,
                           bool resizable) noexcept;
  void ApplySizeConstraints(const X11WindowState& state) noexcept;
  void ApplyInitialWindowMode(::Window w, ::Window root,
                              const WindowDesc& desc) noexcept;
  void SendNetWmStateMessage(::Window root, ::Window w, long action,
                             Atom state1) noexcept;
  u64 FindWindowId(::Window xid) const noexcept;

  ::Display* m_Display {nullptr};
  Atom m_WmDeleteWindow {0};
  Atom m_WmProtocols {0};
  Atom m_NetWmState {0};
  Atom m_NetWmStateFullscreen {0};
  Atom m_NetWmStateMaximizedHorz {0};
  Atom m_NetWmStateMaximizedVert {0};
  Atom m_NetWmStateHidden {0};
  Atom m_NetWmStateAbove {0};
  Atom m_MotifWmHints {0};
  u64 m_NextId {0};

  ::std::unordered_map<u64, X11WindowState> m_Windows;
  ::std::unordered_map<::Window, u64> m_WindowByXid;
  ::std::vector<StagedEvent> m_Staged;
};

Unique<IWindowsBackend> CreateXlibWindowsBackend() noexcept;

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_X11
