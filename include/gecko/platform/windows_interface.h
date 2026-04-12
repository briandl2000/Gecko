#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/window.h"

namespace gecko::platform {

class IWindowsBackend
{
public:
  virtual ~IWindowsBackend() = default;

  [[nodiscard]]
  GECKO_API static Unique<IWindowsBackend> Create(
      const PlatformConfig& cfg) noexcept;

  // ── Window management ────────────────────────────────────────

  GECKO_API virtual bool CreateWindow(const WindowDesc& desc,
                                      WindowHandle& outWindow) noexcept = 0;

  GECKO_API virtual void DestroyWindow(WindowHandle window) noexcept = 0;

  GECKO_API virtual bool IsWindowAlive(WindowHandle window) const noexcept = 0;

  GECKO_API virtual bool RequestClose(WindowHandle window) noexcept = 0;

  // ── Window properties ────────────────────────────────────────

  GECKO_API virtual Extent2D GetClientSize(
      WindowHandle window) const noexcept = 0;

  GECKO_API virtual void SetClientSize(WindowHandle window,
                                       Extent2D size) noexcept = 0;

  GECKO_API virtual void SetTitle(WindowHandle window,
                                  const char* title) noexcept = 0;

  GECKO_API virtual const char* GetTitle(
      WindowHandle window) const noexcept = 0;

  GECKO_API virtual void SetPosition(WindowHandle window,
                                     math::Int2 pos) noexcept = 0;

  GECKO_API virtual math::Int2 GetPosition(
      WindowHandle window) const noexcept = 0;

  GECKO_API virtual DpiInfo GetDpi(WindowHandle window) const noexcept = 0;

  GECKO_API virtual NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept = 0;

  // ── Window state ─────────────────────────────────────────────

  GECKO_API virtual void SetWindowState(WindowHandle window,
                                        WindowState state) noexcept = 0;

  GECKO_API virtual WindowState GetWindowState(
      WindowHandle window) const noexcept = 0;

  GECKO_API virtual void SetDecorated(WindowHandle window,
                                      bool decorated) noexcept = 0;

  GECKO_API virtual bool IsDecorated(WindowHandle window) const noexcept = 0;

  GECKO_API virtual void RequestFocus(WindowHandle window) noexcept = 0;

  // ── Cursor ───────────────────────────────────────────────────

  GECKO_API virtual void SetCursorMode(WindowHandle window,
                                       CursorMode mode) noexcept = 0;

  GECKO_API virtual CursorMode GetCursorMode(
      WindowHandle window) const noexcept = 0;

  // ── Event pump ───────────────────────────────────────────────
  //
  // Process pending OS events and enqueue them on the global event bus
  // using the provided emitter.  Call once per frame via PlatformContext.

  GECKO_API virtual void PumpEvents(
      const gecko::EventEmitter& emitter) noexcept = 0;

private:
};

}  // namespace gecko::platform
