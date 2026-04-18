#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <string>
#include <unordered_map>
#include <vector>
#include <Windows.h>

// Windows.h defines CreateWindow as a macro (CreateWindowA/W), which collides
// with our IWindowsBackend::CreateWindow virtual method.
#undef CreateWindow

#include "gecko/core/services/events.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/windows_interface.h"

namespace gecko::platform {

class Win32WindowsBackend final : public IWindowsBackend
{
public:
  Win32WindowsBackend() noexcept;
  ~Win32WindowsBackend() noexcept override;

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
  void SetModalFrameCallback(ModalFrameFn callback,
                             void* userData) noexcept override;

private:
  struct Win32WindowEntry
  {
    WindowDesc Desc {};
    HWND Hwnd {nullptr};
    Extent2D ClientSize {};
    math::Int2 Position {};
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
    ::std::string TitleStorage;
    // Saved style/position for fullscreen restoration.
    DWORD SavedStyle {0};
    DWORD SavedExStyle {0};
    RECT SavedRect {};
  };

  struct StagedEvent
  {
    gecko::EventCode Code {};
    union PayloadUnion
    {
      events::WindowClosedPayload Closed;
      events::WindowCloseRequestedPayload CloseRequested;
      events::WindowResizedPayload Resized;
      events::WindowDpiChangedPayload DpiChanged;
      events::WindowKeyPayload Key;
      events::WindowCharPayload Char;
      events::WindowMouseMovePayload MouseMove;
      events::WindowMouseButtonPayload MouseButton;
      events::WindowMouseWheelPayload MouseWheel;
      events::WindowFocusChangedPayload FocusChanged;
      events::WindowMovedPayload Moved;
      events::WindowStateChangedPayload StateChanged;
      PayloadUnion() noexcept
      {
        ::std::memset(this, 0, sizeof(PayloadUnion));
      }
    } Data;
    u32 PayloadSize {0};
  };

  Win32WindowEntry* FindEntry(WindowHandle window) noexcept;
  const Win32WindowEntry* FindEntry(WindowHandle window) const noexcept;
  Win32WindowEntry* FindByHwnd(HWND hwnd) noexcept;
  WindowHandle HandleFromHwnd(HWND hwnd) const noexcept;

  void StageEvent(const StagedEvent& ev) noexcept;
  void FlushStagedEvents() noexcept;
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  void ApplyDecorations(HWND hwnd, bool decorated, bool resizable) noexcept;
  DWORD MakeStyle(const WindowDesc& desc) const noexcept;

  static constexpr UINT_PTR kModalTimerId {1};
  static constexpr UINT kModalTimerIntervalMs {16};

  u64 m_NextId {0};
  ATOM m_WndClass {0};
  ::std::unordered_map<u64, Win32WindowEntry> m_Windows;
  ::std::vector<StagedEvent> m_Staged;

  // Emitter pointer held during PumpEvents so WM_TIMER can flush events
  // while Windows runs its internal modal drag/resize loop.
  const gecko::EventEmitter* m_CurrentEmitter {nullptr};

  ModalFrameFn m_ModalFrameCallback {nullptr};
  void* m_ModalFrameUserData {nullptr};

  // We need the backend pointer inside the static WndProc.
  // We store a global instance pointer because all windows share one backend.
  static Win32WindowsBackend* s_Instance;
};

Unique<IWindowsBackend> CreateWin32WindowsBackend() noexcept;

}  // namespace gecko::platform

#endif  // _WIN32
