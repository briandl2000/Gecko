#pragma once

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>
#include <wayland-cursor.h>

#if defined(GECKO_HAS_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#endif

#include "gecko/core/services/events.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/windows_interface.h"

#ifdef GECKO_HAVE_XDG_DECORATION
#include "xdg-decoration-client-protocol.h"
#else
struct ::zxdg_decoration_manager_v1;
struct ::zxdg_toplevel_decoration_v1;
#endif
#include "xdg-shell-client-protocol.h"

namespace gecko::platform {

// -- Per-window state ---------------------------------------------------

struct WaylandWindowState
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
  bool Alive {true};
  u64 GeckoId {0};

  // Wayland objects for this window.
  ::wl_surface* Surface {nullptr};
  ::xdg_surface* XdgSurface {nullptr};
  ::xdg_toplevel* Toplevel {nullptr};
  ::zxdg_toplevel_decoration_v1* Decoration {nullptr};

  // Pending configure state from the compositor.
  i32 PendingWidth {0};
  i32 PendingHeight {0};
  bool ConfigurePending {false};
  u32 PendingSerial {0};

  // Track whether the first configure has been received.
  bool Configured {false};
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

// -- Toplevel listener data ---------------------------------------------

struct ToplevelListenerData
{
  class WaylandWindowsBackend* Backend {nullptr};
  WaylandWindowState* State {nullptr};
};

// -- Backend class ------------------------------------------------------

class WaylandWindowsBackend final : public IWindowsBackend
{
public:
  WaylandWindowsBackend() noexcept;
  ~WaylandWindowsBackend() noexcept override;

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

  // Wayland callbacks need access to internals.
  void OnRegistryGlobal(::wl_registry* registry, u32 name,
                        const char* interface, u32 version) noexcept;
  void OnRegistryGlobalRemove(::wl_registry* registry, u32 name) noexcept;

  void OnXdgSurfaceConfigure(WaylandWindowState* ws, u32 serial) noexcept;
  void OnToplevelConfigure(WaylandWindowState* ws, i32 width, i32 height,
                           wl_array* states) noexcept;
  void OnToplevelClose(WaylandWindowState* ws) noexcept;

  void OnSeatCapabilities(::wl_seat* seat, u32 caps) noexcept;

  // Keyboard callbacks
  void OnKeyboardKeymap(::wl_keyboard* kb, u32 format, i32 fd,
                        u32 size) noexcept;
  void OnKeyboardEnter(::wl_keyboard* kb, u32 serial, ::wl_surface* surface,
                       wl_array* keys) noexcept;
  void OnKeyboardLeave(::wl_keyboard* kb, u32 serial,
                       ::wl_surface* surface) noexcept;
  void OnKeyboardKey(::wl_keyboard* kb, u32 serial, u32 time, u32 key,
                     u32 state) noexcept;
  void OnKeyboardModifiers(::wl_keyboard* kb, u32 serial, u32 modsDepressed,
                           u32 modsLatched, u32 modsLocked, u32 group) noexcept;

  // Pointer callbacks
  void OnPointerEnter(::wl_pointer* pointer, u32 serial, ::wl_surface* surface,
                      wl_fixed_t sx, wl_fixed_t sy) noexcept;
  void OnPointerLeave(::wl_pointer* pointer, u32 serial,
                      ::wl_surface* surface) noexcept;
  void OnPointerMotion(::wl_pointer* pointer, u32 time, wl_fixed_t sx,
                       wl_fixed_t sy) noexcept;
  void OnPointerButton(::wl_pointer* pointer, u32 serial, u32 time, u32 button,
                       u32 state) noexcept;
  void OnPointerAxis(::wl_pointer* pointer, u32 time, u32 axis,
                     wl_fixed_t value) noexcept;

private:
  u64 FindWindowBySurface(::wl_surface* surface) const noexcept;
  void ApplyCursorVisibility(WaylandWindowState& ws) noexcept;
  void AttachBlankBuffer(WaylandWindowState& ws) noexcept;

  ::wl_display* m_Display {nullptr};
  ::wl_registry* m_Registry {nullptr};
  ::wl_compositor* m_Compositor {nullptr};
  ::xdg_wm_base* m_WmBase {nullptr};
  ::wl_seat* m_Seat {nullptr};
  ::wl_shm* m_Shm {nullptr};
  ::wl_keyboard* m_Keyboard {nullptr};
  ::wl_pointer* m_Pointer {nullptr};
  ::zxdg_decoration_manager_v1* m_DecorationManager {nullptr};
  ::wl_cursor_theme* m_CursorTheme {nullptr};
  ::wl_surface* m_CursorSurface {nullptr};

#if defined(GECKO_HAS_XKBCOMMON)
  ::xkb_context* m_XkbContext {nullptr};
  ::xkb_keymap* m_XkbKeymap {nullptr};
  ::xkb_state* m_XkbState {nullptr};
#endif

  u64 m_NextId {0};
  u32 m_LastPointerSerial {0};
  u64 m_FocusedKeyboard {0};
  u64 m_FocusedPointer {0};

  ::std::unordered_map<u64, WaylandWindowState> m_Windows;
  ::std::unordered_map<::wl_surface*, u64> m_WindowBySurface;
  ::std::vector<StagedEvent> m_Staged;
};

Unique<IWindowsBackend> CreateWaylandWindowsBackend() noexcept;

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_WAYLAND
