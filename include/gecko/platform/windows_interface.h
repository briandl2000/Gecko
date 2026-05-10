#pragma once

/// @file
/// Backend interface for the platform window system (`IWindowsBackend`).
///
/// Concrete implementations live under `src/platform/<backend>/` and are
/// constructed via `IWindowsBackend::Create`. The high-level `IWindows`
/// service (see `gecko/platform/window.h` and the platform module) wraps
/// this interface for application code; most users should not call
/// `IWindowsBackend` directly.

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/window.h"

// <windows.h> defines CreateWindow as a macro that expands to
// CreateWindowA / CreateWindowW. That collides with our IWindowsBackend
// API. Drop the macro defensively so our header survives any include
// order. Win32 backend code uses CreateWindowExA/W directly, so this
// does not affect engine internals.
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef CreateWindowA
#undef CreateWindowA
#endif
#ifdef CreateWindowW
#undef CreateWindowW
#endif

namespace gecko::platform {

class IWindowsBackend
{
public:
  virtual ~IWindowsBackend() = default;

  [[nodiscard]]
  GECKO_API static Unique<IWindowsBackend> Create(const PlatformConfig& cfg) noexcept;

  // -- Window management ----------------------------------------

  [[nodiscard]]
  GECKO_API virtual WindowHandle CreateWindow(const WindowDesc& desc) noexcept = 0;

  GECKO_API virtual void DestroyWindow(WindowHandle window) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual bool IsWindowAlive(WindowHandle window) const noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual bool RequestClose(WindowHandle window) noexcept = 0;

  // -- Window properties ----------------------------------------

  [[nodiscard]]
  GECKO_API virtual Extent2D GetClientSize(WindowHandle window) const noexcept = 0;

  GECKO_API virtual void SetClientSize(WindowHandle window, Extent2D size) noexcept = 0;

  GECKO_API virtual void SetTitle(WindowHandle window, const char* title) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual const char* GetTitle(WindowHandle window) const noexcept = 0;

  GECKO_API virtual void SetPosition(WindowHandle window, math::Int2 pos) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual math::Int2 GetPosition(WindowHandle window) const noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual DpiInfo GetDpi(WindowHandle window) const noexcept = 0;

  /// Pointer to the underlying native window plus its display/connection.
  [[nodiscard]]
  GECKO_API virtual NativeWindowHandle GetNativeWindowHandle(WindowHandle window) const noexcept = 0;

  // -- Window state ---------------------------------------------

  GECKO_API virtual void SetWindowState(WindowHandle window, WindowState state) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual WindowState GetWindowState(WindowHandle window) const noexcept = 0;

  GECKO_API virtual void SetDecorated(WindowHandle window, bool decorated) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual bool IsDecorated(WindowHandle window) const noexcept = 0;

  GECKO_API virtual void RequestFocus(WindowHandle window) noexcept = 0;

  // -- Resizability ---------------------------------------------

  GECKO_API virtual void SetResizable(WindowHandle window, bool resizable) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual bool IsResizable(WindowHandle window) const noexcept = 0;

  // -- Window mode ----------------------------------------------

  GECKO_API virtual void SetWindowMode(WindowHandle window, WindowMode mode) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual WindowMode GetWindowMode(WindowHandle window) const noexcept = 0;

  // -- Title-bar button control ---------------------------------

  GECKO_API virtual void SetWindowButtons(WindowHandle window, WindowButtons buttons) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual WindowButtons GetWindowButtons(WindowHandle window) const noexcept = 0;

  // -- Size constraints -----------------------------------------

  GECKO_API virtual void SetMinSize(WindowHandle window, Extent2D size) noexcept = 0;

  GECKO_API virtual void SetMaxSize(WindowHandle window, Extent2D size) noexcept = 0;

  // -- Always on top --------------------------------------------

  GECKO_API virtual void SetAlwaysOnTop(WindowHandle window, bool topmost) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual bool IsAlwaysOnTop(WindowHandle window) const noexcept = 0;

  // -- Cursor ---------------------------------------------------

  GECKO_API virtual void SetCursorMode(WindowHandle window, CursorMode mode) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual CursorMode GetCursorMode(WindowHandle window) const noexcept = 0;

  // -- Event pump -----------------------------------------------
  //
  // Process pending OS events and enqueue them on the global event bus
  // using the provided emitter.  Call once per frame via PlatformContext.

  GECKO_API virtual void PumpEvents(const gecko::EventEmitter& emitter) noexcept = 0;

  // -- Modal frame callback -------------------------------------
  //
  // On platforms with modal event loops (Win32 drag/resize), the backend
  // calls this callback at regular intervals so the application can
  // continue updating and rendering while the OS blocks PumpEvents.
  //
  // The callback should perform one frame of work (dispatch events,
  // update, render) but must NOT call PumpEvents.

  using ModalFrameFn = void (*)(void* userData);

  GECKO_API virtual void SetModalFrameCallback(ModalFrameFn callback, void* userData) noexcept
  {
    (void)callback;
    (void)userData;
  }

private:
};

}  // namespace gecko::platform
