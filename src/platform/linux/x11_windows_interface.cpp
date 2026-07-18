#include "x11_windows_backend.h"

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)

#include "../private/labels.h"
#include "../private/platform_utils.h"
#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/input_codes.h"
#include "x11_key_map.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

namespace gecko::platform {

namespace {
int X11ErrorHandler(::Display* /*display*/, ::XErrorEvent* event)
{
  GECKO_WARN(labels::General, "X11 error: request={}, error={}, serial={}", event->request_code, event->error_code,
             event->serial);
  return 0;
}
}  // namespace

// -- Constructor / Destructor -------------------------------------------

X11WindowsBackend::X11WindowsBackend() noexcept
{
  m_Display = ::XOpenDisplay(nullptr);
  if (!m_Display)
  {
    GECKO_ERROR(labels::General, "Failed to open X display");
    return;
  }

  ::XSetErrorHandler(X11ErrorHandler);

  m_WmDeleteWindow = ::XInternAtom(m_Display, "WM_DELETE_WINDOW", False);
  m_WmProtocols = ::XInternAtom(m_Display, "WM_PROTOCOLS", False);
  m_NetWmState = ::XInternAtom(m_Display, "_NET_WM_STATE", False);
  m_NetWmStateFullscreen = ::XInternAtom(m_Display, "_NET_WM_STATE_FULLSCREEN", False);
  m_NetWmStateMaximizedHorz = ::XInternAtom(m_Display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  m_NetWmStateMaximizedVert = ::XInternAtom(m_Display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  m_NetWmStateHidden = ::XInternAtom(m_Display, "_NET_WM_STATE_HIDDEN", False);
  m_NetWmStateAbove = ::XInternAtom(m_Display, "_NET_WM_STATE_ABOVE", False);
  m_MotifWmHints = ::XInternAtom(m_Display, "_MOTIF_WM_HINTS", False);

  GECKO_INFO(labels::General, "Initialized X11 windows backend (display={})", m_Display);
}

X11WindowsBackend::~X11WindowsBackend() noexcept
{
  // Destroy any remaining windows before closing the display.
  for (auto& [id, state] : m_Windows)
  {
    if (m_Display && state.WindowId != 0)
      ::XDestroyWindow(m_Display, state.WindowId);
  }
  m_Windows.clear();
  m_WindowByXid.clear();

  if (m_Display)
  {
    ::XCloseDisplay(m_Display);
    m_Display = nullptr;
  }
}

WindowHandle X11WindowsBackend::CreateWindow(const WindowDesc& desc) noexcept
{
  GECKO_SCOPE(labels::General);

  if (!m_Display)
    return {};

  const int screen = DefaultScreen(m_Display);
  const ::Window root = RootWindow(m_Display, screen);

  const unsigned int width = desc.Size.X > 0 ? static_cast<unsigned int>(desc.Size.X) : 1280U;
  const unsigned int height = desc.Size.Y > 0 ? static_cast<unsigned int>(desc.Size.Y) : 720U;

  WindowDesc appliedDesc = desc;
  appliedDesc.Size.X = static_cast<i32>(width);
  appliedDesc.Size.Y = static_cast<i32>(height);

  const ::Window w = ::XCreateSimpleWindow(m_Display, root, 0, 0, width, height, 0, BlackPixel(m_Display, screen),
                                           WhitePixel(m_Display, screen));
  if (w == 0)
  {
    GECKO_ERROR(labels::General, "XCreateSimpleWindow failed");
    return {};
  }

  long mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
              PointerMotionMask | FocusChangeMask | EnterWindowMask | LeaveWindowMask;
  ::XSelectInput(m_Display, w, mask);

  ::XStoreName(m_Display, w, desc.Title ? desc.Title : "Gecko");

  ApplyResizableHint(w, appliedDesc);
  ApplyInitialWindowMode(w, root, appliedDesc);

  // Apply decoration setting from the window description.
  if (!desc.Decorated && appliedDesc.Mode == WindowMode::Windowed)
    ApplyMotifDecorations(w, false);

  // Enable window manager close events.
  ::XSetWMProtocols(m_Display, w, &m_WmDeleteWindow, 1);

  if (desc.Visible)
    ::XMapWindow(m_Display, w);

  // Some WMs only apply fullscreen state reliably after mapping.
  if (appliedDesc.Visible && appliedDesc.Mode != WindowMode::Windowed)
  {
    SendNetWmStateMessage(root, w, /*add*/ 1, m_NetWmStateFullscreen);
  }

  ::XFlush(m_Display);

  const u64 id = ++m_NextId;

  X11WindowState st;
  st.Desc = appliedDesc;
  st.TitleStorage = appliedDesc.Title ? appliedDesc.Title : "";
  st.Desc.Title = st.TitleStorage.c_str();
  st.ClientSize = Extent2D {static_cast<u32>(width), static_cast<u32>(height)};
  st.Decorated = desc.Decorated;
  st.Resizable = desc.Resizable;
  st.Mode = desc.Mode;
  st.Buttons = desc.Buttons;
  st.State = desc.Visible ? platform::WindowState::Normal : platform::WindowState::Hidden;
  st.WindowId = w;

  auto [it, ok] = m_Windows.emplace(id, Move(st));
  it->second.Desc.Title = it->second.TitleStorage.c_str();
  m_WindowByXid.emplace(w, id);

  // Apply button restrictions (Motif functions) after mapping.
  if (desc.Buttons != WindowButtons::All)
    ApplyMotifFunctions(w, desc.Buttons, desc.Resizable);

  GECKO_INFO(labels::Window, "Created X11 window id={}, xid={}, size={}x{}", (unsigned long long)id,
             (unsigned long)w, width, height);
  return WindowHandle {id};
}

void X11WindowsBackend::DestroyWindow(WindowHandle window) noexcept
{
  GECKO_SCOPE(labels::General);

  if (!window.IsValid())
    return;

  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  if (m_Display && it->second.WindowId != 0)
  {
    GECKO_DEBUG(labels::Window, "Destroying X11 window id={}, xid={}", (unsigned long long)window.Id,
                (unsigned long)it->second.WindowId);
    m_WindowByXid.erase(it->second.WindowId);
    ::XDestroyWindow(m_Display, it->second.WindowId);
    ::XFlush(m_Display);
  }

  m_Staged.push_back(MakeStagedEvent(events::WindowClosed, events::WindowClosedPayload {window, NowNsSafe()}));

  m_Windows.erase(it);
}

bool X11WindowsBackend::IsWindowAlive(WindowHandle window) const noexcept
{
  if (!window.IsValid())
    return false;
  return m_Windows.find(window.Id) != m_Windows.end();
}

bool X11WindowsBackend::RequestClose(WindowHandle window) noexcept
{
  GECKO_SCOPE(labels::General);

  if (!IsWindowAlive(window))
    return false;

  m_Staged.push_back(
      MakeStagedEvent(events::WindowCloseRequested, events::WindowCloseRequestedPayload {window, NowNsSafe()}));
  return true;
}

void X11WindowsBackend::PumpEvents(const gecko::EventEmitter& emitter) noexcept
{
  GECKO_SCOPE(labels::General);

  // Flush deferred events from RequestClose / DestroyWindow first.
  for (const auto& ev : m_Staged)
    gecko::SendEvent(emitter, ev.Code, gecko::EventView {ev.PayloadStorage, ev.PayloadSize});
  m_Staged.clear();

  if (!m_Display)
    return;

  int eventCount = 0;
  while (::XPending(m_Display) > 0)
  {
    GECKO_PROFILE_NAMED(labels::General, "X11::HandleEvent");
    ::XEvent event;
    ::XNextEvent(m_Display, &event);
    eventCount++;

    const u64 now = NowNsSafe();

    switch (event.type)
    {
    case ClientMessage: {
      if (event.xclient.message_type == m_WmProtocols && event.xclient.format == 32 &&
          static_cast<Atom>(event.xclient.data.l[0]) == m_WmDeleteWindow)
      {
        const u64 id = FindWindowId(event.xclient.window);
        if (id != WindowHandle::InvalidId)
        {
          gecko::SendEvent(emitter, events::WindowCloseRequested,
                           events::WindowCloseRequestedPayload {WindowHandle {id}, now});
        }
      }
    }
    break;

    case ConfigureNotify: {
      const u64 id = FindWindowId(event.xconfigure.window);
      if (id == WindowHandle::InvalidId)
        break;

      auto it = m_Windows.find(id);
      if (it == m_Windows.end())
        break;

      const u32 newW = static_cast<u32>(event.xconfigure.width);
      const u32 newH = static_cast<u32>(event.xconfigure.height);
      if (it->second.ClientSize.Width != newW || it->second.ClientSize.Height != newH)
      {
        it->second.ClientSize = Extent2D {newW, newH};
        gecko::SendEvent(emitter, events::WindowResized,
                         events::WindowResizedPayload {WindowHandle {id}, now, newW, newH});
      }

      const i32 newX = static_cast<i32>(event.xconfigure.x);
      const i32 newY = static_cast<i32>(event.xconfigure.y);
      if (it->second.Position.X != newX || it->second.Position.Y != newY)
      {
        it->second.Position = math::Int2 {newX, newY};
        gecko::SendEvent(emitter, events::WindowMoved, events::WindowMovedPayload {WindowHandle {id}, now, newX, newY});
      }
    }
    break;

    case KeyPress:
    case KeyRelease: {
      const u64 id = FindWindowId(event.xkey.window);
      if (id == WindowHandle::InvalidId)
        break;

      const bool down = (event.type == KeyPress);
      const KeySym keysym = ::XLookupKeysym(&event.xkey, 0);
      const KeyCode key = X11KeySymToKeyCode(keysym);

      gecko::SendEvent(emitter, events::WindowKey,
                       events::WindowKeyPayload {WindowHandle {id}, now, key, down ? u8(1) : u8(0), 0});

      if (down)
      {
        char buf[64];
        KeySym sym;
        XComposeStatus compose {};
        const int len = ::XLookupString(&event.xkey, buf, sizeof(buf), &sym, &compose);
        // Decode the UTF-8 (XLookupString returns Latin-1, but for the
        // ASCII subset that's identical to UTF-8 -- full Unicode requires
        // an XIM input context which we don't currently set up). Skip
        // C0 control characters except tab/CR/LF.
        if (len > 0)
        {
          const auto* bytes = reinterpret_cast<const unsigned char*>(buf);
          int i = 0;
          while (i < len)
          {
            gecko::u32 cp = 0;
            int consumed = 1;
            const unsigned char b0 = bytes[i];
            if (b0 < 0x80)
            {
              cp = b0;
            }
            else if ((b0 & 0xE0) == 0xC0 && i + 1 < len)
            {
              cp = gecko::u32(b0 & 0x1F) << 6 | gecko::u32(bytes[i + 1] & 0x3F);
              consumed = 2;
            }
            else if ((b0 & 0xF0) == 0xE0 && i + 2 < len)
            {
              cp = gecko::u32(b0 & 0x0F) << 12 | gecko::u32(bytes[i + 1] & 0x3F) << 6 |
                   gecko::u32(bytes[i + 2] & 0x3F);
              consumed = 3;
            }
            else if ((b0 & 0xF8) == 0xF0 && i + 3 < len)
            {
              cp = gecko::u32(b0 & 0x07) << 18 | gecko::u32(bytes[i + 1] & 0x3F) << 12 |
                   gecko::u32(bytes[i + 2] & 0x3F) << 6 | gecko::u32(bytes[i + 3] & 0x3F);
              consumed = 4;
            }
            else
            {
              // Latin-1 byte (XLookupString fallback) outside ASCII.
              cp = b0;
            }
            i += consumed;

            // Skip C0 controls but keep tab/CR/LF.
            if (cp < 32 && cp != '\t' && cp != '\n' && cp != '\r')
              continue;
            if (cp == 127)
              continue;

            gecko::SendEvent(emitter, events::WindowChar, events::WindowCharPayload {WindowHandle {id}, now, cp});
          }
        }
      }
    }
    break;

    case MotionNotify: {
      const u64 id = FindWindowId(event.xmotion.window);
      if (id == WindowHandle::InvalidId)
        break;

      gecko::SendEvent(emitter, events::WindowMouseMove,
                       events::WindowMouseMovePayload {WindowHandle {id}, now, static_cast<i32>(event.xmotion.x),
                                                       static_cast<i32>(event.xmotion.y)});
    }
    break;

    case ButtonPress:
    case ButtonRelease: {
      const u64 id = FindWindowId(event.xbutton.window);
      if (id == WindowHandle::InvalidId)
        break;

      const bool down = (event.type == ButtonPress);
      const unsigned int btn = event.xbutton.button;

      if (btn == 4 || btn == 5 || btn == 6 || btn == 7)
      {
        if (down)
        {
          float dx = 0.0F;
          float dy = 0.0F;
          if (btn == 4)
            dy = 1.0F;
          else if (btn == 5)
            dy = -1.0F;
          else if (btn == 6)
            dx = 1.0F;
          else if (btn == 7)
            dx = -1.0F;

          gecko::SendEvent(emitter, events::WindowMouseWheel,
                           events::WindowMouseWheelPayload {WindowHandle {id}, now, dx, dy});
        }
      }
      else
      {
        gecko::SendEvent(emitter, events::WindowMouseButton,
                         events::WindowMouseButtonPayload {WindowHandle {id}, now, X11ButtonToMouseButton(btn),
                                                           down ? u8(1) : u8(0)});
      }
    }
    break;

    case FocusIn:
    case FocusOut: {
      const u64 id = FindWindowId(event.xfocus.window);
      if (id != WindowHandle::InvalidId)
      {
        const u8 focused = (event.type == FocusIn) ? 1 : 0;
        gecko::SendEvent(emitter, events::WindowFocusChanged,
                         events::WindowFocusChangedPayload {WindowHandle {id}, now, focused});
      }
    }
    break;

    case EnterNotify: {
      const u64 id = FindWindowId(event.xcrossing.window);
      if (id != WindowHandle::InvalidId)
      {
        gecko::SendEvent(emitter, events::WindowMouseEntered,
                         events::WindowMouseEnteredPayload {WindowHandle {id}, now});
      }
    }
    break;

    case LeaveNotify: {
      const u64 id = FindWindowId(event.xcrossing.window);
      if (id != WindowHandle::InvalidId)
      {
        gecko::SendEvent(emitter, events::WindowMouseExited, events::WindowMouseExitedPayload {WindowHandle {id}, now});
      }
    }
    break;

    default:
      break;
    }
  }

  if (eventCount > 0)
    GECKO_TRACE(labels::Input, "Pumped {} X11 events", eventCount);
}

Extent2D X11WindowsBackend::GetClientSize(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return Extent2D {};
  return it->second.ClientSize;
}

void X11WindowsBackend::SetTitle(WindowHandle window, const char* title) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.TitleStorage = title ? title : "";
  it->second.Desc.Title = it->second.TitleStorage.c_str();
  if (m_Display && it->second.WindowId != 0 && title)
    ::XStoreName(m_Display, it->second.WindowId, title);
}

DpiInfo X11WindowsBackend::GetDpi(WindowHandle /*window*/) const noexcept
{
  // TODO: query Xft.dpi resource or compute from XRRGetOutputInfo physical
  // dimensions and resolution.
  GECKO_DEBUG(labels::Window, "GetDpi: not yet implemented on X11, "
                              "returning default 96 DPI / 1.0x scale");
  return DpiInfo {};
}

NativeWindowHandle X11WindowsBackend::GetNativeWindowHandle(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return NativeWindowHandle {};

  NativeWindowHandle nh;
  nh.Backend = DisplayBackendKind::Xlib;
  nh.Display = m_Display;
  nh.Handle = reinterpret_cast<void*>(static_cast<usize>(it->second.WindowId));
  return nh;
}

void X11WindowsBackend::SetClientSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  ::XResizeWindow(m_Display, it->second.WindowId, size.Width, size.Height);
  ::XFlush(m_Display);
  // Actual size update happens via ConfigureNotify in PumpEvents.
}

const char* X11WindowsBackend::GetTitle(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return "";
  return it->second.TitleStorage.c_str();
}

void X11WindowsBackend::SetPosition(WindowHandle window, math::Int2 pos) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  ::XMoveWindow(m_Display, it->second.WindowId, pos.X, pos.Y);
  ::XFlush(m_Display);
  // Actual position update happens via ConfigureNotify in PumpEvents.
}

math::Int2 X11WindowsBackend::GetPosition(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return math::Int2 {0, 0};
  return it->second.Position;
}

void X11WindowsBackend::SetWindowState(WindowHandle window, platform::WindowState state) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  const auto oldState = it->second.State;
  if (oldState == state)
    return;

  const ::Window xid = it->second.WindowId;
  const int screen = DefaultScreen(m_Display);
  const ::Window root = RootWindow(m_Display, screen);

  switch (state)
  {
  case platform::WindowState::Normal:
    // Remove maximized state if previously maximized.
    if (oldState == platform::WindowState::Maximized)
    {
      SendNetWmStateMessage(root, xid, /*remove*/ 0, m_NetWmStateMaximizedHorz);
      SendNetWmStateMessage(root, xid, /*remove*/ 0, m_NetWmStateMaximizedVert);
    }
    ::XMapWindow(m_Display, xid);
    break;

  case platform::WindowState::Minimized:
    ::XIconifyWindow(m_Display, xid, screen);
    break;

  case platform::WindowState::Maximized:
    ::XMapWindow(m_Display, xid);
    SendNetWmStateMessage(root, xid, /*add*/ 1, m_NetWmStateMaximizedHorz);
    SendNetWmStateMessage(root, xid, /*add*/ 1, m_NetWmStateMaximizedVert);
    break;

  case platform::WindowState::Hidden:
    ::XUnmapWindow(m_Display, xid);
    break;
  }

  ::XFlush(m_Display);
  it->second.State = state;

  m_Staged.push_back(MakeStagedEvent(events::WindowStateChanged,
                                     events::WindowStateChangedPayload {window, NowNsSafe(), oldState, state}));
}

platform::WindowState X11WindowsBackend::GetWindowState(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return platform::WindowState::Normal;
  return it->second.State;
}

void X11WindowsBackend::SetDecorated(WindowHandle window, bool decorated) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  it->second.Decorated = decorated;
  ApplyMotifDecorations(it->second.WindowId, decorated);
  ::XFlush(m_Display);
}

bool X11WindowsBackend::IsDecorated(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return true;
  return it->second.Decorated;
}

void X11WindowsBackend::RequestFocus(WindowHandle window) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  ::XRaiseWindow(m_Display, it->second.WindowId);
  ::XSetInputFocus(m_Display, it->second.WindowId, RevertToParent, CurrentTime);
  ::XFlush(m_Display);
}

void X11WindowsBackend::SetResizable(WindowHandle window, bool resizable) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  it->second.Resizable = resizable;

  XSizeHints hints {};
  if (resizable)
  {
    // Remove min=max constraint by setting to wide-open range.
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = 1;
    hints.min_height = 1;
    hints.max_width = 32767;
    hints.max_height = 32767;

    // Restore any user-specified constraints.
    if (it->second.MinSize.Width > 0)
      hints.min_width = static_cast<int>(it->second.MinSize.Width);
    if (it->second.MinSize.Height > 0)
      hints.min_height = static_cast<int>(it->second.MinSize.Height);
    if (it->second.MaxSize.Width > 0)
      hints.max_width = static_cast<int>(it->second.MaxSize.Width);
    if (it->second.MaxSize.Height > 0)
      hints.max_height = static_cast<int>(it->second.MaxSize.Height);
  }
  else
  {
    // Lock to current size.
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = static_cast<int>(it->second.ClientSize.Width);
    hints.min_height = static_cast<int>(it->second.ClientSize.Height);
    hints.max_width = static_cast<int>(it->second.ClientSize.Width);
    hints.max_height = static_cast<int>(it->second.ClientSize.Height);
  }
  ::XSetWMNormalHints(m_Display, it->second.WindowId, &hints);
  ApplyMotifFunctions(it->second.WindowId, it->second.Buttons, resizable);
  ::XFlush(m_Display);
}

bool X11WindowsBackend::IsResizable(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return true;
  return it->second.Resizable;
}

void X11WindowsBackend::SetWindowMode(WindowHandle window, WindowMode mode) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  const WindowMode old = it->second.Mode;
  if (old == mode)
    return;

  const int screen = DefaultScreen(m_Display);
  const ::Window root = RootWindow(m_Display, screen);
  const ::Window xid = it->second.WindowId;

  // Remove current fullscreen state if active.
  if (old == WindowMode::Fullscreen || old == WindowMode::BorderlessFullscreen)
  {
    SendNetWmStateMessage(root, xid, /*remove*/ 0, m_NetWmStateFullscreen);
  }

  switch (mode)
  {
  case WindowMode::Windowed:
    ApplyMotifDecorations(xid, it->second.Decorated);
    break;

  case WindowMode::Fullscreen:
  case WindowMode::BorderlessFullscreen:
    ApplyMotifDecorations(xid, false);
    SendNetWmStateMessage(root, xid, /*add*/ 1, m_NetWmStateFullscreen);
    break;
  }

  ::XFlush(m_Display);
  it->second.Mode = mode;
}

WindowMode X11WindowsBackend::GetWindowMode(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return WindowMode::Windowed;
  return it->second.Mode;
}

void X11WindowsBackend::SetWindowButtons(WindowHandle window, WindowButtons buttons) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  it->second.Buttons = buttons;
  ApplyMotifFunctions(it->second.WindowId, buttons, it->second.Resizable);
  ::XFlush(m_Display);
}

WindowButtons X11WindowsBackend::GetWindowButtons(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return WindowButtons::All;
  return it->second.Buttons;
}

void X11WindowsBackend::SetMinSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  it->second.MinSize = size;
  if (it->second.Resizable)
    ApplySizeConstraints(it->second);
}

void X11WindowsBackend::SetMaxSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  it->second.MaxSize = size;
  if (it->second.Resizable)
    ApplySizeConstraints(it->second);
}

void X11WindowsBackend::SetAlwaysOnTop(WindowHandle window, bool topmost) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  it->second.AlwaysOnTop = topmost;

  const int screen = DefaultScreen(m_Display);
  const ::Window root = RootWindow(m_Display, screen);
  SendNetWmStateMessage(root, it->second.WindowId, topmost ? 1 : 0, m_NetWmStateAbove);
  ::XFlush(m_Display);
}

bool X11WindowsBackend::IsAlwaysOnTop(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return false;
  return it->second.AlwaysOnTop;
}

void X11WindowsBackend::SetCursorMode(WindowHandle window, CursorMode mode) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !m_Display)
    return;

  it->second.Cursor = mode;

  const ::Window xid = it->second.WindowId;
  if (mode == CursorMode::Hidden || mode == CursorMode::Locked)
  {
    // Create an invisible cursor.
    Pixmap blank = ::XCreatePixmap(m_Display, xid, 1, 1, 1);
    XColor dummy {};
    Cursor invisible = ::XCreatePixmapCursor(m_Display, blank, blank, &dummy, &dummy, 0, 0);
    ::XDefineCursor(m_Display, xid, invisible);
    ::XFreeCursor(m_Display, invisible);
    ::XFreePixmap(m_Display, blank);

    if (mode == CursorMode::Locked)
    {
      ::XGrabPointer(m_Display, xid, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
                     GrabModeAsync, xid, 0L, CurrentTime);
    }
  }
  else
  {
    ::XUndefineCursor(m_Display, xid);
    ::XUngrabPointer(m_Display, CurrentTime);
  }

  ::XFlush(m_Display);
}

CursorMode X11WindowsBackend::GetCursorMode(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return CursorMode::Normal;
  return it->second.Cursor;
}

void X11WindowsBackend::ApplyResizableHint(::Window w, const WindowDesc& desc) noexcept
{
  if (!m_Display || w == 0)
    return;

  if (desc.Resizable)
    return;

  XSizeHints hints {};
  hints.flags = PMinSize | PMaxSize;
  hints.min_width = static_cast<int>(desc.Size.X);
  hints.min_height = static_cast<int>(desc.Size.Y);
  hints.max_width = static_cast<int>(desc.Size.X);
  hints.max_height = static_cast<int>(desc.Size.Y);
  ::XSetWMNormalHints(m_Display, w, &hints);
}

void X11WindowsBackend::ApplyMotifDecorations(::Window w, bool enabled) noexcept
{
  if (!m_Display || w == 0 || m_MotifWmHints == 0)
    return;

  MwmHints hints {};
  hints.flags = MwmHintsDecorations;
  hints.decorations = enabled ? MwmDecorAll : 0UL;

  ::XChangeProperty(m_Display, w, m_MotifWmHints, m_MotifWmHints, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&hints), static_cast<int>(sizeof(hints) / sizeof(long)));
}

void X11WindowsBackend::ApplyMotifFunctions(::Window w, WindowButtons buttons, bool resizable) noexcept
{
  if (!m_Display || w == 0 || m_MotifWmHints == 0)
    return;

  // Read existing hints so we preserve decorations flags.
  MwmHints hints {};
  Atom actualType {};
  int actualFormat {};
  unsigned long nItems {};
  unsigned long bytesAfter {};
  unsigned char* propData {};

  if (::XGetWindowProperty(m_Display, w, m_MotifWmHints, 0, sizeof(MwmHints) / sizeof(long), False, m_MotifWmHints,
                           &actualType, &actualFormat, &nItems, &bytesAfter, &propData) == 0 &&
      propData && nItems >= 5)
  {
    hints = *reinterpret_cast<MwmHints*>(propData);
    ::XFree(propData);
  }

  hints.flags |= MwmHintsFunctions;
  // Start with move (always allowed).
  unsigned long funcs = MwmFuncMove;
  if (resizable)
    funcs |= MwmFuncResize;
  if (gecko::Any(buttons & WindowButtons::Minimize))
    funcs |= MwmFuncMinimize;
  if (gecko::Any(buttons & WindowButtons::Maximize))
    funcs |= MwmFuncMaximize;
  if (gecko::Any(buttons & WindowButtons::Close))
    funcs |= MwmFuncClose;
  hints.functions = funcs;

  ::XChangeProperty(m_Display, w, m_MotifWmHints, m_MotifWmHints, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&hints), static_cast<int>(sizeof(hints) / sizeof(long)));
}

void X11WindowsBackend::ApplySizeConstraints(const X11WindowState& state) noexcept
{
  if (!m_Display || state.WindowId == 0)
    return;

  XSizeHints hints {};
  hints.flags = PMinSize | PMaxSize;
  hints.min_width = state.MinSize.Width > 0 ? static_cast<int>(state.MinSize.Width) : 1;
  hints.min_height = state.MinSize.Height > 0 ? static_cast<int>(state.MinSize.Height) : 1;
  hints.max_width = state.MaxSize.Width > 0 ? static_cast<int>(state.MaxSize.Width) : 32767;
  hints.max_height = state.MaxSize.Height > 0 ? static_cast<int>(state.MaxSize.Height) : 32767;
  ::XSetWMNormalHints(m_Display, state.WindowId, &hints);
  ::XFlush(m_Display);
}

void X11WindowsBackend::ApplyInitialWindowMode(::Window w, ::Window root, const WindowDesc& desc) noexcept
{
  if (!m_Display || w == 0)
    return;

  switch (desc.Mode)
  {
  case WindowMode::Windowed:
    // Default decorations; no EWMH fullscreen hint.
    ApplyMotifDecorations(w, true);
    break;

  case WindowMode::Fullscreen:
  case WindowMode::BorderlessFullscreen: {
    // Best-effort: make it borderless and ask WM for fullscreen.
    ApplyMotifDecorations(w, false);

    if (m_NetWmState != 0 && m_NetWmStateFullscreen != 0)
    {
      Atom atoms[1] = {m_NetWmStateFullscreen};
      ::XChangeProperty(m_Display, w, m_NetWmState, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(atoms), 1);

      // Also send a client message (some WMs prefer this path).
      SendNetWmStateMessage(root, w, /*add*/ 1, m_NetWmStateFullscreen);
    }
  }
  break;
  }
}

void X11WindowsBackend::SendNetWmStateMessage(::Window root, ::Window w, long action, Atom state1) noexcept
{
  if (!m_Display || root == 0 || w == 0 || m_NetWmState == 0 || state1 == 0)
    return;

  ::XEvent ev {};
  ev.xclient.type = ClientMessage;
  ev.xclient.serial = 0;
  ev.xclient.send_event = True;
  ev.xclient.display = m_Display;
  ev.xclient.window = w;
  ev.xclient.message_type = m_NetWmState;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = action;  // 1 = add, 0 = remove, 2 = toggle
  ev.xclient.data.l[1] = static_cast<long>(state1);
  ev.xclient.data.l[2] = 0;
  ev.xclient.data.l[3] = 1;  // normal source indication
  ev.xclient.data.l[4] = 0;

  ::XSendEvent(m_Display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}

u64 X11WindowsBackend::FindWindowId(::Window xid) const noexcept
{
  auto it = m_WindowByXid.find(xid);
  if (it == m_WindowByXid.end())
    return WindowHandle::InvalidId;
  return it->second;
}

Unique<IWindowsBackend> CreateXlibWindowsBackend() noexcept
{
  return CreateUnique<X11WindowsBackend>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_X11
