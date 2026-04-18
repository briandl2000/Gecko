#include "win32_windows_backend.h"

#if defined(_WIN32)

#include "../private/labels.h"
#include "../private/platform_utils.h"
#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/input_codes.h"

#include <cstring>
#include <shellscalingapi.h>
#include <windowsx.h>

#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "User32.lib")

namespace gecko::platform {

namespace {

// ── Key mapping ────────────────────────────────────────────────────────
// KeyCode values match Win32 Virtual-Key codes, so mapping is identity
// for all defined codes. Unknown VK codes map to KeyCode::Unknown.

KeyCode VkToKeyCode(WPARAM vk) noexcept
{
  const auto code = static_cast<u16>(vk);
  // Verify the code falls within a recognised range.
  // Letters/digits, function keys, OEM keys, numpad, control keys:
  // instead of an exhaustive switch we check the enum is valid via a
  // quick bounds check and rely on the 1:1 mapping.
  switch (code)
  {
  case 0x08:  // VK_BACK
  case 0x09:  // VK_TAB
  case 0x0C:  // VK_CLEAR
  case 0x0D:  // VK_RETURN
  case 0x10:  // VK_SHIFT
  case 0x11:  // VK_CONTROL
  case 0x12:  // VK_MENU (Alt)
  case 0x13:  // VK_PAUSE
  case 0x14:  // VK_CAPITAL
  case 0x1B:  // VK_ESCAPE
  case 0x20:  // VK_SPACE
  case 0x21:  // VK_PRIOR
  case 0x22:  // VK_NEXT
  case 0x23:  // VK_END
  case 0x24:  // VK_HOME
  case 0x25:  // VK_LEFT
  case 0x26:  // VK_UP
  case 0x27:  // VK_RIGHT
  case 0x28:  // VK_DOWN
  case 0x2C:  // VK_SNAPSHOT (PrintScreen)
  case 0x2D:  // VK_INSERT
  case 0x2E:  // VK_DELETE
  case 0x5B:  // VK_LWIN
  case 0x5C:  // VK_RWIN
  case 0x5D:  // VK_APPS (Menu)
  case 0x90:  // VK_NUMLOCK
  case 0x91:  // VK_SCROLL
  case 0xA0:  // VK_LSHIFT
  case 0xA1:  // VK_RSHIFT
  case 0xA2:  // VK_LCONTROL
  case 0xA3:  // VK_RCONTROL
  case 0xA4:  // VK_LMENU
  case 0xA5:  // VK_RMENU
  case 0xBA:  // VK_OEM_1 (Semicolon)
  case 0xBB:  // VK_OEM_PLUS
  case 0xBC:  // VK_OEM_COMMA
  case 0xBD:  // VK_OEM_MINUS
  case 0xBE:  // VK_OEM_PERIOD
  case 0xBF:  // VK_OEM_2 (Slash)
  case 0xC0:  // VK_OEM_3 (GraveAccent)
  case 0xDB:  // VK_OEM_4 (LeftBracket)
  case 0xDC:  // VK_OEM_5 (Backslash)
  case 0xDD:  // VK_OEM_6 (RightBracket)
  case 0xDE:  // VK_OEM_7 (Apostrophe)
    return static_cast<KeyCode>(code);

  default:
    // Digits 0x30–0x39, Letters 0x41–0x5A
    if ((code >= 0x30 && code <= 0x39) || (code >= 0x41 && code <= 0x5A))
      return static_cast<KeyCode>(code);
    // Numpad 0x60–0x6F
    if (code >= 0x60 && code <= 0x6F)
      return static_cast<KeyCode>(code);
    // Function keys F1–F12 (0x70–0x7B)
    if (code >= 0x70 && code <= 0x7B)
      return static_cast<KeyCode>(code);
    return KeyCode::Unknown;
  }
}

MouseButton WmButtonToMouseButton(UINT msg) noexcept
{
  switch (msg)
  {
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
    return MouseButton::Left;
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
    return MouseButton::Right;
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP:
    return MouseButton::Middle;
  case WM_XBUTTONDOWN:
  case WM_XBUTTONUP:
    return MouseButton::X1;  // refined in caller via HIWORD(wParam)
  default:
    return MouseButton::Left;
  }
}

constexpr wchar_t kWndClassName[] = L"GeckoWindowClass";

}  // namespace

Win32WindowsBackend* Win32WindowsBackend::s_Instance = nullptr;

// ── Constructor / Destructor ───────────────────────────────────────────

Win32WindowsBackend::Win32WindowsBackend() noexcept
{
  s_Instance = this;

  WNDCLASSEXW wc {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = ::GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.lpszClassName = kWndClassName;
  m_WndClass = ::RegisterClassExW(&wc);

  GECKO_INFO(labels::General, "Win32WindowsBackend: initialized");
}

Win32WindowsBackend::~Win32WindowsBackend() noexcept
{
  for (auto& [id, entry] : m_Windows)
  {
    if (entry.Hwnd)
      ::DestroyWindow(entry.Hwnd);
  }
  m_Windows.clear();

  if (m_WndClass)
    ::UnregisterClassW(kWndClassName, ::GetModuleHandleW(nullptr));

  if (s_Instance == this)
    s_Instance = nullptr;
}

// ── Window management ──────────────────────────────────────────────────

DWORD Win32WindowsBackend::MakeStyle(const WindowDesc& desc) const noexcept
{
  DWORD style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

  if (desc.Mode == WindowMode::Fullscreen ||
      desc.Mode == WindowMode::BorderlessFullscreen)
  {
    style |= WS_POPUP;
  }
  else if (desc.Decorated)
  {
    style |= WS_OVERLAPPEDWINDOW;
    if (!desc.Resizable)
      style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  else
  {
    style |= WS_POPUP;
    if (desc.Resizable)
      style |= WS_THICKFRAME;
  }

  if (desc.Visible)
    style |= WS_VISIBLE;

  return style;
}

void Win32WindowsBackend::ApplyDecorations(HWND hwnd, bool decorated,
                                           bool resizable) noexcept
{
  DWORD style = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_STYLE));

  if (decorated)
  {
    style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (resizable)
      style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
  }
  else
  {
    style &= ~(WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME |
               WS_MAXIMIZEBOX);
    style |= WS_POPUP;
  }

  ::SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(style));
  ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

WindowHandle Win32WindowsBackend::CreateWindow(const WindowDesc& desc) noexcept
{
  GECKO_FUNC(labels::General);

  const DWORD style = MakeStyle(desc);
  const DWORD exStyle = WS_EX_APPWINDOW;

  // Adjust from client size to window size
  RECT rect {};
  rect.right = desc.Size.X;
  rect.bottom = desc.Size.Y;
  ::AdjustWindowRectEx(&rect, style, FALSE, exStyle);
  const int windowWidth = rect.right - rect.left;
  const int windowHeight = rect.bottom - rect.top;

  // Convert title to wide string
  const int titleLen =
      ::MultiByteToWideChar(CP_UTF8, 0, desc.Title, -1, nullptr, 0);
  ::std::vector<wchar_t> wTitle(static_cast<size_t>(titleLen));
  ::MultiByteToWideChar(CP_UTF8, 0, desc.Title, -1, wTitle.data(), titleLen);

  HWND hwnd =
      ::CreateWindowExW(exStyle, kWndClassName, wTitle.data(), style,
                        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
                        nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);

  if (!hwnd)
  {
    GECKO_ERROR(labels::General, "Win32WindowsBackend: CreateWindowExW failed");
    return {};
  }

  const u64 id = ++m_NextId;

  Win32WindowEntry entry;
  entry.Desc = desc;
  entry.Hwnd = hwnd;
  entry.ClientSize = {static_cast<u32>(desc.Size.X),
                      static_cast<u32>(desc.Size.Y)};
  entry.Decorated = desc.Decorated;
  entry.Resizable = desc.Resizable;
  entry.Mode = desc.Mode;
  entry.Buttons = desc.Buttons;
  entry.State = desc.Visible ? platform::WindowState::Normal
                             : platform::WindowState::Hidden;
  entry.Alive = true;
  entry.TitleStorage = desc.Title ? desc.Title : "";

  // Query actual position
  RECT winRect;
  if (::GetWindowRect(hwnd, &winRect))
    entry.Position = {static_cast<i32>(winRect.left),
                      static_cast<i32>(winRect.top)};

  m_Windows.emplace(id, ::std::move(entry));

  // Store the handle id in the window user data for WndProc lookup
  ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(id));

  if (desc.Visible)
    ::ShowWindow(hwnd, SW_SHOW);

  GECKO_INFO(labels::General,
             "Win32WindowsBackend: created window id=%llu hwnd=%p",
             static_cast<unsigned long long>(id), static_cast<void*>(hwnd));
  return WindowHandle {id};
}

void Win32WindowsBackend::DestroyWindow(WindowHandle window) noexcept
{
  GECKO_FUNC(labels::General);
  auto* entry = FindEntry(window);
  if (!entry)
    return;

  if (entry->Hwnd)
    ::DestroyWindow(entry->Hwnd);

  entry->Alive = false;

  StagedEvent ev;
  ev.Code = events::WindowClosed;
  ev.Data.Closed = {window, NowNsSafe()};
  ev.PayloadSize = static_cast<u32>(sizeof(events::WindowClosedPayload));
  m_Staged.push_back(ev);

  m_Windows.erase(window.Id);
}

bool Win32WindowsBackend::IsWindowAlive(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  return entry && entry->Alive;
}

bool Win32WindowsBackend::RequestClose(WindowHandle window) noexcept
{
  GECKO_FUNC(labels::General);
  auto* entry = FindEntry(window);
  if (!entry || !entry->Alive)
    return false;

  StagedEvent ev;
  ev.Code = events::WindowCloseRequested;
  ev.Data.CloseRequested = {window, NowNsSafe()};
  ev.PayloadSize =
      static_cast<u32>(sizeof(events::WindowCloseRequestedPayload));
  m_Staged.push_back(ev);
  return true;
}

// ── Window properties ──────────────────────────────────────────────────

Extent2D Win32WindowsBackend::GetClientSize(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return Extent2D {};
  return entry->ClientSize;
}

void Win32WindowsBackend::SetClientSize(WindowHandle window,
                                        Extent2D size) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  entry->ClientSize = size;

  DWORD style = static_cast<DWORD>(::GetWindowLongPtrW(entry->Hwnd, GWL_STYLE));
  DWORD exStyle =
      static_cast<DWORD>(::GetWindowLongPtrW(entry->Hwnd, GWL_EXSTYLE));

  RECT rect {};
  rect.right = static_cast<LONG>(size.Width);
  rect.bottom = static_cast<LONG>(size.Height);
  ::AdjustWindowRectEx(&rect, style, FALSE, exStyle);

  ::SetWindowPos(entry->Hwnd, nullptr, 0, 0, rect.right - rect.left,
                 rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32WindowsBackend::SetTitle(WindowHandle window,
                                   const char* title) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  entry->TitleStorage = title ? title : "";

  const int len = ::MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
  ::std::vector<wchar_t> wTitle(static_cast<size_t>(len));
  ::MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle.data(), len);
  ::SetWindowTextW(entry->Hwnd, wTitle.data());
}

const char* Win32WindowsBackend::GetTitle(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return "";
  return entry->TitleStorage.c_str();
}

void Win32WindowsBackend::SetPosition(WindowHandle window,
                                      math::Int2 pos) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  entry->Position = pos;
  ::SetWindowPos(entry->Hwnd, nullptr, pos.X, pos.Y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

math::Int2 Win32WindowsBackend::GetPosition(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return math::Int2 {0, 0};
  return entry->Position;
}

DpiInfo Win32WindowsBackend::GetDpi(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return DpiInfo {};

  HMONITOR hMon = ::MonitorFromWindow(entry->Hwnd, MONITOR_DEFAULTTONEAREST);
  UINT dpiX = 96;
  UINT dpiY = 96;
  if (SUCCEEDED(::GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
    return DpiInfo {static_cast<u32>(dpiX), static_cast<float>(dpiX) / 96.0F};
  return DpiInfo {};
}

NativeWindowHandle Win32WindowsBackend::GetNativeWindowHandle(
    WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return NativeWindowHandle {};

  NativeWindowHandle native;
  native.Backend = DisplayBackendKind::Win32;
  native.Handle = static_cast<void*>(entry->Hwnd);
  native.Display = nullptr;
  return native;
}

// ── Window state ───────────────────────────────────────────────────────

void Win32WindowsBackend::SetWindowState(WindowHandle window,
                                         platform::WindowState state) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  const auto oldState = entry->State;
  if (oldState == state)
    return;

  switch (state)
  {
  case platform::WindowState::Normal:
    ::ShowWindow(entry->Hwnd, SW_RESTORE);
    break;
  case platform::WindowState::Minimized:
    ::ShowWindow(entry->Hwnd, SW_MINIMIZE);
    break;
  case platform::WindowState::Maximized:
    ::ShowWindow(entry->Hwnd, SW_MAXIMIZE);
    break;
  case platform::WindowState::Hidden:
    ::ShowWindow(entry->Hwnd, SW_HIDE);
    break;
  }

  entry->State = state;

  StagedEvent ev;
  ev.Code = events::WindowStateChanged;
  ev.Data.StateChanged = {window, NowNsSafe(), oldState, state};
  ev.PayloadSize = static_cast<u32>(sizeof(events::WindowStateChangedPayload));
  m_Staged.push_back(ev);
}

platform::WindowState Win32WindowsBackend::GetWindowState(
    WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return platform::WindowState::Normal;
  return entry->State;
}

void Win32WindowsBackend::SetDecorated(WindowHandle window,
                                       bool decorated) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  entry->Decorated = decorated;
  ApplyDecorations(entry->Hwnd, decorated, entry->Desc.Resizable);
}

bool Win32WindowsBackend::IsDecorated(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return true;
  return entry->Decorated;
}

void Win32WindowsBackend::RequestFocus(WindowHandle window) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  ::SetForegroundWindow(entry->Hwnd);
  ::SetFocus(entry->Hwnd);
}

void Win32WindowsBackend::SetResizable(WindowHandle window,
                                       bool resizable) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  entry->Resizable = resizable;
  ApplyDecorations(entry->Hwnd, entry->Decorated, resizable);
}

bool Win32WindowsBackend::IsResizable(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return true;
  return entry->Resizable;
}

void Win32WindowsBackend::SetWindowMode(WindowHandle window,
                                        WindowMode mode) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  const WindowMode old = entry->Mode;
  if (old == mode)
    return;

  if (mode == WindowMode::Fullscreen ||
      mode == WindowMode::BorderlessFullscreen)
  {
    // Save current style and position for restoration.
    entry->SavedStyle =
        static_cast<DWORD>(::GetWindowLongPtrW(entry->Hwnd, GWL_STYLE));
    entry->SavedExStyle =
        static_cast<DWORD>(::GetWindowLongPtrW(entry->Hwnd, GWL_EXSTYLE));
    ::GetWindowRect(entry->Hwnd, &entry->SavedRect);

    // Go borderless fullscreen on the current monitor.
    HMONITOR hMon = ::MonitorFromWindow(entry->Hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi {};
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfoW(hMon, &mi);

    ::SetWindowLongPtrW(entry->Hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    ::SetWindowPos(entry->Hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                   mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   SWP_FRAMECHANGED | SWP_NOACTIVATE);
  }
  else
  {
    // Restore saved style and position.
    if (entry->SavedStyle != 0)
    {
      ::SetWindowLongPtrW(entry->Hwnd, GWL_STYLE, entry->SavedStyle);
      ::SetWindowLongPtrW(entry->Hwnd, GWL_EXSTYLE, entry->SavedExStyle);
      ::SetWindowPos(entry->Hwnd, nullptr, entry->SavedRect.left,
                     entry->SavedRect.top,
                     entry->SavedRect.right - entry->SavedRect.left,
                     entry->SavedRect.bottom - entry->SavedRect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else
    {
      ApplyDecorations(entry->Hwnd, entry->Decorated, entry->Resizable);
    }
  }

  entry->Mode = mode;
}

WindowMode Win32WindowsBackend::GetWindowMode(
    WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return WindowMode::Windowed;
  return entry->Mode;
}

void Win32WindowsBackend::SetWindowButtons(WindowHandle window,
                                           WindowButtons buttons) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  entry->Buttons = buttons;

  HMENU sysMenu = ::GetSystemMenu(entry->Hwnd, FALSE);
  if (!sysMenu)
    return;

  // Reset the system menu first.
  ::GetSystemMenu(entry->Hwnd, TRUE);
  sysMenu = ::GetSystemMenu(entry->Hwnd, FALSE);
  if (!sysMenu)
    return;

  if (!::gecko::Any(buttons & WindowButtons::Close))
    ::EnableMenuItem(sysMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
  if (!::gecko::Any(buttons & WindowButtons::Minimize))
    ::EnableMenuItem(sysMenu, SC_MINIMIZE, MF_BYCOMMAND | MF_GRAYED);
  if (!::gecko::Any(buttons & WindowButtons::Maximize))
    ::EnableMenuItem(sysMenu, SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);

  // Also toggle the WS_MINIMIZEBOX/WS_MAXIMIZEBOX style bits.
  LONG_PTR style = ::GetWindowLongPtrW(entry->Hwnd, GWL_STYLE);
  if (::gecko::Any(buttons & WindowButtons::Minimize))
    style |= WS_MINIMIZEBOX;
  else
    style &= ~WS_MINIMIZEBOX;
  if (::gecko::Any(buttons & WindowButtons::Maximize))
    style |= WS_MAXIMIZEBOX;
  else
    style &= ~WS_MAXIMIZEBOX;
  ::SetWindowLongPtrW(entry->Hwnd, GWL_STYLE, style);

  ::DrawMenuBar(entry->Hwnd);
  ::SetWindowPos(entry->Hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

WindowButtons Win32WindowsBackend::GetWindowButtons(
    WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return WindowButtons::All;
  return entry->Buttons;
}

void Win32WindowsBackend::SetMinSize(WindowHandle window,
                                     Extent2D size) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry)
    return;
  entry->MinSize = size;
}

void Win32WindowsBackend::SetMaxSize(WindowHandle window,
                                     Extent2D size) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry)
    return;
  entry->MaxSize = size;
}

void Win32WindowsBackend::SetAlwaysOnTop(WindowHandle window,
                                         bool topmost) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  entry->AlwaysOnTop = topmost;
  ::SetWindowPos(entry->Hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0,
                 0, SWP_NOMOVE | SWP_NOSIZE);
}

bool Win32WindowsBackend::IsAlwaysOnTop(WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return false;
  return entry->AlwaysOnTop;
}

// ── Cursor ─────────────────────────────────────────────────────────────

void Win32WindowsBackend::SetCursorMode(WindowHandle window,
                                        CursorMode mode) noexcept
{
  auto* entry = FindEntry(window);
  if (!entry || !entry->Hwnd)
    return;

  const auto oldMode = entry->Cursor;
  entry->Cursor = mode;

  // Undo previous lock clip
  if (oldMode == CursorMode::Locked)
    ::ClipCursor(nullptr);

  // ShowCursor uses a global counter — only call when the cursor visibility
  // actually needs to change, otherwise the counter drifts.
  const bool wasVisible = (oldMode == CursorMode::Normal);
  const bool nowVisible = (mode == CursorMode::Normal);

  if (wasVisible && !nowVisible)
    ::ShowCursor(FALSE);
  else if (!wasVisible && nowVisible)
    ::ShowCursor(TRUE);

  if (mode == CursorMode::Locked)
  {
    RECT clipRect;
    if (::GetClientRect(entry->Hwnd, &clipRect))
    {
      POINT topLeft {clipRect.left, clipRect.top};
      POINT bottomRight {clipRect.right, clipRect.bottom};
      ::ClientToScreen(entry->Hwnd, &topLeft);
      ::ClientToScreen(entry->Hwnd, &bottomRight);
      RECT screenRect {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
      ::ClipCursor(&screenRect);
    }
  }
}

CursorMode Win32WindowsBackend::GetCursorMode(
    WindowHandle window) const noexcept
{
  const auto* entry = FindEntry(window);
  if (!entry)
    return CursorMode::Normal;
  return entry->Cursor;
}

// ── Event pump ─────────────────────────────────────────────────────────

void Win32WindowsBackend::PumpEvents(
    const gecko::EventEmitter& emitter) noexcept
{
  m_CurrentEmitter = &emitter;

  MSG msg;
  while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
  {
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);
  }

  FlushStagedEvents();
  m_CurrentEmitter = nullptr;
}

// ── WndProc ────────────────────────────────────────────────────────────

LRESULT CALLBACK Win32WindowsBackend::WndProc(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam)
{
  if (!s_Instance)
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);

  auto* self = s_Instance;
  const WindowHandle wh = self->HandleFromHwnd(hwnd);
  auto* entry = self->FindByHwnd(hwnd);

  switch (msg)
  {
  case WM_CLOSE: {
    if (wh.IsValid())
    {
      StagedEvent ev;
      ev.Code = events::WindowCloseRequested;
      ev.Data.CloseRequested = {wh, NowNsSafe()};
      ev.PayloadSize =
          static_cast<u32>(sizeof(events::WindowCloseRequestedPayload));
      self->m_Staged.push_back(ev);
    }
    return 0;  // prevent default DestroyWindow
  }

  case WM_SIZE: {
    if (!entry)
      break;
    const u32 w = LOWORD(lParam);
    const u32 h = HIWORD(lParam);
    entry->ClientSize = {w, h};

    StagedEvent ev;
    ev.Code = events::WindowResized;
    ev.Data.Resized = {wh, NowNsSafe(), w, h};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowResizedPayload));
    self->m_Staged.push_back(ev);

    // Track state changes from WM_SIZE wParam
    platform::WindowState newState = entry->State;
    if (wParam == SIZE_MINIMIZED)
      newState = platform::WindowState::Minimized;
    else if (wParam == SIZE_MAXIMIZED)
      newState = platform::WindowState::Maximized;
    else if (wParam == SIZE_RESTORED)
      newState = platform::WindowState::Normal;

    if (newState != entry->State)
    {
      const auto oldState = entry->State;
      entry->State = newState;

      StagedEvent stEv;
      stEv.Code = events::WindowStateChanged;
      stEv.Data.StateChanged = {wh, NowNsSafe(), oldState, newState};
      stEv.PayloadSize =
          static_cast<u32>(sizeof(events::WindowStateChangedPayload));
      self->m_Staged.push_back(stEv);
    }
    break;
  }

  case WM_MOVE: {
    if (!entry)
      break;
    const i32 x = static_cast<i32>(static_cast<short>(LOWORD(lParam)));
    const i32 y = static_cast<i32>(static_cast<short>(HIWORD(lParam)));
    entry->Position = {x, y};

    StagedEvent ev;
    ev.Code = events::WindowMoved;
    ev.Data.Moved = {wh, NowNsSafe(), x, y};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowMovedPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    if (!entry)
      break;
    const KeyCode key = VkToKeyCode(wParam);
    const u8 repeat = (lParam & 0x40000000) ? 1 : 0;

    StagedEvent ev;
    ev.Code = events::WindowKey;
    ev.Data.Key = {wh, NowNsSafe(), key, 1, repeat};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowKeyPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_KEYUP:
  case WM_SYSKEYUP: {
    if (!entry)
      break;
    const KeyCode key = VkToKeyCode(wParam);

    StagedEvent ev;
    ev.Code = events::WindowKey;
    ev.Data.Key = {wh, NowNsSafe(), key, 0, 0};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowKeyPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_CHAR: {
    if (!entry)
      break;
    // Skip control characters
    if (wParam < 32 && wParam != '\t' && wParam != '\n' && wParam != '\r')
      break;

    StagedEvent ev;
    ev.Code = events::WindowChar;
    ev.Data.Char = {wh, NowNsSafe(), static_cast<u32>(wParam)};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowCharPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_MOUSEMOVE: {
    if (!entry)
      break;
    const i32 x = GET_X_LPARAM(lParam);
    const i32 y = GET_Y_LPARAM(lParam);

    StagedEvent ev;
    ev.Code = events::WindowMouseMove;
    ev.Data.MouseMove = {wh, NowNsSafe(), x, y};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowMouseMovePayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_XBUTTONDOWN: {
    if (!entry)
      break;
    MouseButton btn = WmButtonToMouseButton(msg);
    if (msg == WM_XBUTTONDOWN && GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
      btn = MouseButton::X2;

    StagedEvent ev;
    ev.Code = events::WindowMouseButton;
    ev.Data.MouseButton = {wh, NowNsSafe(), btn, 1};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowMouseButtonPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
  case WM_XBUTTONUP: {
    if (!entry)
      break;
    MouseButton btn = WmButtonToMouseButton(msg);
    if (msg == WM_XBUTTONUP && GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
      btn = MouseButton::X2;

    StagedEvent ev;
    ev.Code = events::WindowMouseButton;
    ev.Data.MouseButton = {wh, NowNsSafe(), btn, 0};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowMouseButtonPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_MOUSEWHEEL: {
    if (!entry)
      break;
    const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                        static_cast<float>(WHEEL_DELTA);

    StagedEvent ev;
    ev.Code = events::WindowMouseWheel;
    ev.Data.MouseWheel = {wh, NowNsSafe(), 0.0F, delta};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowMouseWheelPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_MOUSEHWHEEL: {
    if (!entry)
      break;
    const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                        static_cast<float>(WHEEL_DELTA);

    StagedEvent ev;
    ev.Code = events::WindowMouseWheel;
    ev.Data.MouseWheel = {wh, NowNsSafe(), delta, 0.0F};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowMouseWheelPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_SETFOCUS: {
    if (!entry)
      break;
    StagedEvent ev;
    ev.Code = events::WindowFocusChanged;
    ev.Data.FocusChanged = {wh, NowNsSafe(), 1};
    ev.PayloadSize =
        static_cast<u32>(sizeof(events::WindowFocusChangedPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_KILLFOCUS: {
    if (!entry)
      break;
    // Release cursor lock when losing focus
    if (entry->Cursor == CursorMode::Locked)
      ::ClipCursor(nullptr);

    StagedEvent ev;
    ev.Code = events::WindowFocusChanged;
    ev.Data.FocusChanged = {wh, NowNsSafe(), 0};
    ev.PayloadSize =
        static_cast<u32>(sizeof(events::WindowFocusChangedPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_DPICHANGED: {
    if (!entry)
      break;
    const u32 dpi = HIWORD(wParam);
    const float scale = static_cast<float>(dpi) / 96.0F;

    // Resize to suggested rect
    const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
    ::SetWindowPos(entry->Hwnd, nullptr, suggested->left, suggested->top,
                   suggested->right - suggested->left,
                   suggested->bottom - suggested->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);

    StagedEvent ev;
    ev.Code = events::WindowDpiChanged;
    ev.Data.DpiChanged = {wh, NowNsSafe(), dpi, scale};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowDpiChangedPayload));
    self->m_Staged.push_back(ev);
    break;
  }

  case WM_ENTERSIZEMOVE: {
    // Windows enters a modal loop during drag/resize that blocks our
    // PumpEvents.  Start a fast timer so we can keep flushing staged
    // events (resize, move, etc.) while the modal loop is running.
    ::SetTimer(hwnd, kModalTimerId, kModalTimerIntervalMs, nullptr);
    break;
  }

  case WM_EXITSIZEMOVE: {
    ::KillTimer(hwnd, kModalTimerId);
    break;
  }

  case WM_TIMER: {
    if (wParam == kModalTimerId)
    {
      self->FlushStagedEvents();
      if (self->m_ModalFrameCallback)
        self->m_ModalFrameCallback(self->m_ModalFrameUserData);
    }
    break;
  }

  default:
    break;
  }

  return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ── Helpers ────────────────────────────────────────────────────────────

Win32WindowsBackend::Win32WindowEntry* Win32WindowsBackend::FindEntry(
    WindowHandle window) noexcept
{
  if (!window.IsValid())
    return nullptr;
  auto it = m_Windows.find(window.Id);
  return (it != m_Windows.end()) ? &it->second : nullptr;
}

const Win32WindowsBackend::Win32WindowEntry* Win32WindowsBackend::FindEntry(
    WindowHandle window) const noexcept
{
  if (!window.IsValid())
    return nullptr;
  auto it = m_Windows.find(window.Id);
  return (it != m_Windows.end()) ? &it->second : nullptr;
}

Win32WindowsBackend::Win32WindowEntry* Win32WindowsBackend::FindByHwnd(
    HWND hwnd) noexcept
{
  const u64 id = static_cast<u64>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (id == 0)
    return nullptr;
  auto it = m_Windows.find(id);
  return (it != m_Windows.end()) ? &it->second : nullptr;
}

WindowHandle Win32WindowsBackend::HandleFromHwnd(HWND hwnd) const noexcept
{
  const u64 id = static_cast<u64>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  return WindowHandle {id};
}

void Win32WindowsBackend::StageEvent(const StagedEvent& ev) noexcept
{
  m_Staged.push_back(ev);
}

void Win32WindowsBackend::FlushStagedEvents() noexcept
{
  if (!m_CurrentEmitter || m_Staged.empty())
    return;

  for (const auto& ev : m_Staged)
    gecko::SendEvent(*m_CurrentEmitter, ev.Code,
                     gecko::EventView {&ev.Data, ev.PayloadSize});
  m_Staged.clear();
}

void Win32WindowsBackend::SetModalFrameCallback(ModalFrameFn callback,
                                                void* userData) noexcept
{
  m_ModalFrameCallback = callback;
  m_ModalFrameUserData = userData;
}

// ── Factory ────────────────────────────────────────────────────────────

Unique<IWindowsBackend> CreateWin32WindowsBackend() noexcept
{
  return CreateUnique<Win32WindowsBackend>();
}

}  // namespace gecko::platform

#endif  // _WIN32
