#include "gecko/platform/window.h"

#if defined(__linux__) && defined(GECKO_PLATFORM_LINUX_X11)

#include "../labels.h"
#include "../window_backend.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace gecko::platform {

namespace {
static u64 NowNsSafe() noexcept
{
  if (auto* profiler = GetProfiler())
    return profiler->NowNs();
  return 0;
}

struct X11WindowState
{
  WindowDesc Desc {};
  Extent2D ClientSize {};
  ::Window WindowId {0};
};

struct MwmHints
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long inputMode;
  unsigned long status;
};

constexpr unsigned long MWM_HINTS_DECORATIONS = 1UL << 1;
constexpr unsigned long MWM_DECOR_ALL = 1UL;
}  // namespace

class X11WindowBackend final : public IWindowBackend
{
public:
  X11WindowBackend() noexcept = default;

  bool CreateWindow(const WindowDesc& desc,
                    WindowHandle& outWindow) noexcept override
  {
    GECKO_PROF_FUNC(labels::General);

    if (!EnsureDisplay())
      return false;

    const int screen = DefaultScreen(m_Display);
    const ::Window root = RootWindow(m_Display, screen);

    const unsigned int width = desc.Size.Width > 0 ? desc.Size.Width : 1280U;
    const unsigned int height = desc.Size.Height > 0 ? desc.Size.Height : 720U;

    WindowDesc appliedDesc = desc;
    appliedDesc.Size.Width = static_cast<u32>(width);
    appliedDesc.Size.Height = static_cast<u32>(height);

    const ::Window w = XCreateSimpleWindow(m_Display, root, 0, 0, width, height,
                                           0, BlackPixel(m_Display, screen),
                                           WhitePixel(m_Display, screen));
    if (w == 0)
    {
      GECKO_ERROR(labels::General, "XCreateSimpleWindow failed\n");
      return false;
    }

    long mask = ExposureMask | StructureNotifyMask | KeyPressMask |
                KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                PointerMotionMask | FocusChangeMask;
    XSelectInput(m_Display, w, mask);

    XStoreName(m_Display, w, desc.Title ? desc.Title : "Gecko");

    ApplyResizableHint(w, appliedDesc);
    ApplyInitialWindowMode(w, root, appliedDesc);

    // Enable window manager close events.
    XSetWMProtocols(m_Display, w, &m_WmDeleteWindow, 1);

    if (desc.Visible)
      XMapWindow(m_Display, w);

    // Some WMs only apply fullscreen state reliably after mapping.
    if (appliedDesc.Visible && appliedDesc.Mode != WindowMode::Windowed)
    {
      SendNetWmStateMessage(root, w, /*add*/ 1, m_NetWmStateFullscreen);
    }

    XFlush(m_Display);

    const u64 id = ++m_NextId;
    outWindow = WindowHandle {id};

    X11WindowState st;
    st.Desc = appliedDesc;
    st.ClientSize =
        Extent2D {static_cast<u32>(width), static_cast<u32>(height)};
    st.WindowId = w;

    m_Windows.emplace(id, st);
    m_WindowByXid.emplace(w, id);

    GECKO_INFO(labels::Window,
               "Created X11 window id=%llu, xid=%lu, size=%ux%u",
               (unsigned long long)id, (unsigned long)w, width, height);
    return true;
  }

  void DestroyWindow(WindowHandle window) noexcept override
  {
    GECKO_PROF_FUNC(labels::General);

    if (!window.IsValid())
      return;

    auto it = m_Windows.find(window.Id);
    if (it == m_Windows.end())
      return;

    if (m_Display && it->second.WindowId != 0)
    {
      GECKO_DEBUG(labels::Window, "Destroying X11 window id=%llu, xid=%lu",
                  (unsigned long long)window.Id,
                  (unsigned long)it->second.WindowId);
      m_WindowByXid.erase(it->second.WindowId);
      XDestroyWindow(m_Display, it->second.WindowId);
      XFlush(m_Display);
    }

    WindowEvent ev {};
    ev.Kind = WindowEventKind::Closed;
    ev.Window = window;
    ev.TimeNs = NowNsSafe();
    m_Events.push_back(ev);

    m_Windows.erase(it);
  }

  bool IsWindowAlive(WindowHandle window) const noexcept override
  {
    if (!window.IsValid())
      return false;
    return m_Windows.find(window.Id) != m_Windows.end();
  }

  bool RequestClose(WindowHandle window) noexcept override
  {
    GECKO_PROF_FUNC(labels::General);

    if (!IsWindowAlive(window))
      return false;

    WindowEvent ev {};
    ev.Kind = WindowEventKind::CloseRequested;
    ev.Window = window;
    ev.TimeNs = NowNsSafe();
    m_Events.push_back(ev);
    return true;
  }

  void PumpEvents() noexcept override
  {
    GECKO_PROF_FUNC(labels::General);

    if (!m_Display)
      return;

    int eventCount = 0;
    while (XPending(m_Display) > 0)
    {
      XEvent event;
      XNextEvent(m_Display, &event);
      eventCount++;

      const u64 now = NowNsSafe();

      switch (event.type)
      {
      case ClientMessage: {
        if (event.xclient.message_type == m_WmProtocols &&
            event.xclient.format == 32 &&
            static_cast<Atom>(event.xclient.data.l[0]) == m_WmDeleteWindow)
        {
          const u64 id = FindWindowId(event.xclient.window);
          if (id != 0)
          {
            WindowEvent ev {};
            ev.Kind = WindowEventKind::CloseRequested;
            ev.Window = WindowHandle {id};
            ev.TimeNs = now;
            m_Events.push_back(ev);
          }
        }
      }
      break;

      case ConfigureNotify: {
        const u64 id = FindWindowId(event.xconfigure.window);
        if (id == 0)
          break;

        auto it = m_Windows.find(id);
        if (it == m_Windows.end())
          break;

        const u32 newW = static_cast<u32>(event.xconfigure.width);
        const u32 newH = static_cast<u32>(event.xconfigure.height);
        if (it->second.ClientSize.Width != newW ||
            it->second.ClientSize.Height != newH)
        {
          it->second.ClientSize = Extent2D {newW, newH};

          WindowEvent ev {};
          ev.Kind = WindowEventKind::Resized;
          ev.Window = WindowHandle {id};
          ev.TimeNs = now;
          ev.Data.Resize.Width = newW;
          ev.Data.Resize.Height = newH;
          m_Events.push_back(ev);
        }
      }
      break;

      case KeyPress:
      case KeyRelease: {
        const u64 id = FindWindowId(event.xkey.window);
        if (id == 0)
          break;

        const bool down = (event.type == KeyPress);

        KeySym keysym = XLookupKeysym(&event.xkey, 0);

        WindowEvent ev {};
        ev.Kind = WindowEventKind::Key;
        ev.Window = WindowHandle {id};
        ev.TimeNs = now;
        ev.Data.Key.Key = static_cast<u32>(keysym);
        ev.Data.Key.Down = down ? 1U : 0U;
        ev.Data.Key.Repeat = 0U;
        m_Events.push_back(ev);

        if (down)
        {
          // Minimal text input: best-effort ASCII from XLookupString.
          char buf[64];
          KeySym sym;
          XComposeStatus compose {};
          const int len =
              XLookupString(&event.xkey, buf, sizeof(buf), &sym, &compose);
          if (len == 1)
          {
            const unsigned char c = static_cast<unsigned char>(buf[0]);
            if (c >= 32)
            {
              WindowEvent cev {};
              cev.Kind = WindowEventKind::Char;
              cev.Window = WindowHandle {id};
              cev.TimeNs = now;
              cev.Data.Char.Codepoint = static_cast<u32>(c);
              m_Events.push_back(cev);
            }
          }
        }
      }
      break;

      case MotionNotify: {
        const u64 id = FindWindowId(event.xmotion.window);
        if (id == 0)
          break;

        WindowEvent ev {};
        ev.Kind = WindowEventKind::MouseMove;
        ev.Window = WindowHandle {id};
        ev.TimeNs = now;
        ev.Data.MouseMove.X = static_cast<i32>(event.xmotion.x);
        ev.Data.MouseMove.Y = static_cast<i32>(event.xmotion.y);
        m_Events.push_back(ev);
      }
      break;

      case ButtonPress:
      case ButtonRelease: {
        const u64 id = FindWindowId(event.xbutton.window);
        if (id == 0)
          break;

        const bool down = (event.type == ButtonPress);
        const unsigned int btn = event.xbutton.button;

        if (btn == 4 || btn == 5 || btn == 6 || btn == 7)
        {
          if (down)
          {
            WindowEvent ev {};
            ev.Kind = WindowEventKind::MouseWheel;
            ev.Window = WindowHandle {id};
            ev.TimeNs = now;
            ev.Data.MouseWheel.DeltaX = 0.0F;
            ev.Data.MouseWheel.DeltaY = 0.0F;
            if (btn == 4)
              ev.Data.MouseWheel.DeltaY = 1.0F;
            else if (btn == 5)
              ev.Data.MouseWheel.DeltaY = -1.0F;
            else if (btn == 6)
              ev.Data.MouseWheel.DeltaX = 1.0F;
            else if (btn == 7)
              ev.Data.MouseWheel.DeltaX = -1.0F;
            m_Events.push_back(ev);
          }
        }
        else
        {
          WindowEvent ev {};
          ev.Kind = WindowEventKind::MouseButton;
          ev.Window = WindowHandle {id};
          ev.TimeNs = now;
          ev.Data.MouseButton.Button = static_cast<u8>(btn);
          ev.Data.MouseButton.Down = down ? 1U : 0U;
          m_Events.push_back(ev);
        }
      }
      break;

      default:
        break;
      }
    }

    if (eventCount > 0)
    {
      GECKO_TRACE(labels::Input, "Pumped %d X11 events", eventCount);
    }
  }

  bool PollEvent(WindowEvent& outEvent) noexcept override
  {
    if (m_Events.empty())
      return false;
    outEvent = m_Events.front();
    m_Events.pop_front();
    return true;
  }

  Extent2D GetClientSize(WindowHandle window) const noexcept override
  {
    auto it = m_Windows.find(window.Id);
    if (it == m_Windows.end())
      return Extent2D {};
    return it->second.ClientSize;
  }

  void SetTitle(WindowHandle window, const char* title) noexcept override
  {
    auto it = m_Windows.find(window.Id);
    if (it == m_Windows.end())
      return;
    it->second.Desc.Title = title;
    if (m_Display && it->second.WindowId != 0 && title)
      XStoreName(m_Display, it->second.WindowId, title);
  }

  DpiInfo GetDpi(WindowHandle window) const noexcept override
  {
    (void)window;
    return DpiInfo {};
  }

  NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept override
  {
    auto it = m_Windows.find(window.Id);
    if (it == m_Windows.end())
      return NativeWindowHandle {};

    NativeWindowHandle nh;
    nh.Backend = WindowBackendKind::Xlib;
    nh.Display = m_Display;
    nh.Handle =
        reinterpret_cast<void*>(static_cast<uintptr_t>(it->second.WindowId));
    return nh;
  }

private:
  bool EnsureDisplay() noexcept
  {
    if (m_Display)
      return true;

    m_Display = XOpenDisplay(nullptr);
    if (!m_Display)
    {
      GECKO_ERROR(labels::General, "XOpenDisplay failed (no X server?)\n");
      return false;
    }

    m_WmDeleteWindow = XInternAtom(m_Display, "WM_DELETE_WINDOW", False);
    m_WmProtocols = XInternAtom(m_Display, "WM_PROTOCOLS", False);
    m_NetWmState = XInternAtom(m_Display, "_NET_WM_STATE", False);
    m_NetWmStateFullscreen =
        XInternAtom(m_Display, "_NET_WM_STATE_FULLSCREEN", False);
    m_MotifWmHints = XInternAtom(m_Display, "_MOTIF_WM_HINTS", False);
    return true;
  }

  void ApplyResizableHint(::Window w, const WindowDesc& desc) noexcept
  {
    if (!m_Display || w == 0)
      return;

    if (desc.Resizable)
      return;

    XSizeHints hints {};
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = static_cast<int>(desc.Size.Width);
    hints.min_height = static_cast<int>(desc.Size.Height);
    hints.max_width = static_cast<int>(desc.Size.Width);
    hints.max_height = static_cast<int>(desc.Size.Height);
    XSetWMNormalHints(m_Display, w, &hints);
  }

  void ApplyMotifDecorations(::Window w, bool enabled) noexcept
  {
    if (!m_Display || w == 0 || m_MotifWmHints == 0)
      return;

    MwmHints hints {};
    hints.flags = MWM_HINTS_DECORATIONS;
    hints.decorations = enabled ? MWM_DECOR_ALL : 0UL;

    XChangeProperty(m_Display, w, m_MotifWmHints, m_MotifWmHints, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&hints),
                    static_cast<int>(sizeof(hints) / sizeof(long)));
  }

  void ApplyInitialWindowMode(::Window w, ::Window root,
                              const WindowDesc& desc) noexcept
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
        XChangeProperty(m_Display, w, m_NetWmState, XA_ATOM, 32,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(atoms), 1);

        // Also send a client message (some WMs prefer this path).
        SendNetWmStateMessage(root, w, /*add*/ 1, m_NetWmStateFullscreen);
      }
    }
    break;
    }
  }

  void SendNetWmStateMessage(::Window root, ::Window w, long action,
                             Atom state1) noexcept
  {
    if (!m_Display || root == 0 || w == 0 || m_NetWmState == 0 || state1 == 0)
      return;

    XEvent ev {};
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

    XSendEvent(m_Display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  }

  u64 FindWindowId(::Window xid) const noexcept
  {
    auto it = m_WindowByXid.find(xid);
    if (it == m_WindowByXid.end())
      return 0;
    return it->second;
  }

  Display* m_Display {nullptr};
  Atom m_WmDeleteWindow {0};
  Atom m_WmProtocols {0};
  Atom m_NetWmState {0};
  Atom m_NetWmStateFullscreen {0};
  Atom m_MotifWmHints {0};
  u64 m_NextId {0};

  std::unordered_map<u64, X11WindowState> m_Windows;
  std::unordered_map<::Window, u64> m_WindowByXid;
  std::deque<WindowEvent> m_Events;
};

IWindowBackend& GetXlibWindowBackend() noexcept
{
  static X11WindowBackend backend;
  return backend;
}

}  // namespace gecko::platform

#endif  // __linux__ && GECKO_PLATFORM_LINUX_X11
