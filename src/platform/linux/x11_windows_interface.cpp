#include "gecko/platform/window.h"

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)

#include "../private/labels.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/windows_interface.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
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
  std::memcpy(ev.PayloadStorage, &payload, sizeof(T));
  return ev;
}
}  // namespace

class X11WindowsBackend final : public IWindowsBackend
{
public:
  X11WindowsBackend() noexcept
  {
    m_Display = XOpenDisplay(nullptr);
    if (!m_Display)
    {
      GECKO_ERROR(labels::General, "Failed to open X display");
      return;
    }

    m_WmDeleteWindow = XInternAtom(m_Display, "WM_DELETE_WINDOW", False);
    m_WmProtocols = XInternAtom(m_Display, "WM_PROTOCOLS", False);
    m_NetWmState = XInternAtom(m_Display, "_NET_WM_STATE", False);
    m_NetWmStateFullscreen =
        XInternAtom(m_Display, "_NET_WM_STATE_FULLSCREEN", False);
    m_MotifWmHints = XInternAtom(m_Display, "_MOTIF_WM_HINTS", False);

    GECKO_INFO(labels::General, "Initialized X11 windows backend (display=%p)",
               m_Display);
  }

  ~X11WindowsBackend() noexcept override
  {
    // Destroy any remaining windows before closing the display.
    for (auto& [id, state] : m_Windows)
    {
      if (m_Display && state.WindowId != 0)
        XDestroyWindow(m_Display, state.WindowId);
    }
    m_Windows.clear();
    m_WindowByXid.clear();

    if (m_Display)
    {
      XCloseDisplay(m_Display);
      m_Display = nullptr;
    }
  }

  bool CreateWindow(const WindowDesc& desc,
                    WindowHandle& outWindow) noexcept override
  {
    GECKO_FUNC(labels::General);

    if (!m_Display)
      return false;

    const int screen = DefaultScreen(m_Display);
    const ::Window root = RootWindow(m_Display, screen);

    const unsigned int width =
        desc.Size.X > 0 ? static_cast<unsigned int>(desc.Size.X) : 1280U;
    const unsigned int height =
        desc.Size.Y > 0 ? static_cast<unsigned int>(desc.Size.Y) : 720U;

    WindowDesc appliedDesc = desc;
    appliedDesc.Size.X = static_cast<i32>(width);
    appliedDesc.Size.Y = static_cast<i32>(height);

    const ::Window w = XCreateSimpleWindow(m_Display, root, 0, 0, width, height,
                                           0, BlackPixel(m_Display, screen),
                                           WhitePixel(m_Display, screen));
    if (w == 0)
    {
      GECKO_ERROR(labels::General, "XCreateSimpleWindow failed");
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
    GECKO_FUNC(labels::General);

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

    m_Staged.push_back(
        MakeStagedEvent(events::WindowClosed,
                        events::WindowClosedPayload {window, NowNsSafe()}));

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
    GECKO_FUNC(labels::General);

    if (!IsWindowAlive(window))
      return false;

    m_Staged.push_back(MakeStagedEvent(
        events::WindowCloseRequested,
        events::WindowCloseRequestedPayload {window, NowNsSafe()}));
    return true;
  }

  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override
  {
    GECKO_FUNC(labels::General);

    // Flush deferred events from RequestClose / DestroyWindow first.
    for (const auto& ev : m_Staged)
      gecko::PublishEvent(emitter, ev.Code,
                          gecko::EventView {ev.PayloadStorage, ev.PayloadSize});
    m_Staged.clear();

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
            gecko::PublishEvent(
                emitter, events::WindowCloseRequested,
                events::WindowCloseRequestedPayload {WindowHandle {id}, now});
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
          gecko::PublishEvent(emitter, events::WindowResized,
                              events::WindowResizedPayload {WindowHandle {id},
                                                            now, newW, newH});
        }
      }
      break;

      case KeyPress:
      case KeyRelease: {
        const u64 id = FindWindowId(event.xkey.window);
        if (id == 0)
          break;

        const bool down = (event.type == KeyPress);
        const KeySym keysym = XLookupKeysym(&event.xkey, 0);

        gecko::PublishEvent(emitter, events::WindowKey,
                            events::WindowKeyPayload {WindowHandle {id}, now,
                                                      static_cast<u32>(keysym),
                                                      down ? u8(1) : u8(0), 0});

        if (down)
        {
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
              gecko::PublishEvent(
                  emitter, events::WindowChar,
                  events::WindowCharPayload {WindowHandle {id}, now,
                                             static_cast<u32>(c)});
            }
          }
        }
      }
      break;

      case MotionNotify: {
        const u64 id = FindWindowId(event.xmotion.window);
        if (id == 0)
          break;

        gecko::PublishEvent(
            emitter, events::WindowMouseMove,
            events::WindowMouseMovePayload {WindowHandle {id}, now,
                                            static_cast<i32>(event.xmotion.x),
                                            static_cast<i32>(event.xmotion.y)});
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

            gecko::PublishEvent(emitter, events::WindowMouseWheel,
                                events::WindowMouseWheelPayload {
                                    WindowHandle {id}, now, dx, dy});
          }
        }
        else
        {
          gecko::PublishEvent(emitter, events::WindowMouseButton,
                              events::WindowMouseButtonPayload {
                                  WindowHandle {id}, now, static_cast<u8>(btn),
                                  down ? u8(1) : u8(0)});
        }
      }
      break;

      default:
        break;
      }
    }

    if (eventCount > 0)
      GECKO_TRACE(labels::Input, "Pumped %d X11 events", eventCount);
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
    nh.Backend = DisplayBackendKind::Xlib;
    nh.Display = m_Display;
    nh.Handle =
        reinterpret_cast<void*>(static_cast<uintptr_t>(it->second.WindowId));
    return nh;
  }

private:
  void ApplyResizableHint(::Window w, const WindowDesc& desc) noexcept
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
  std::vector<StagedEvent> m_Staged;
};

Unique<IWindowsBackend> CreateXlibWindowsBackend() noexcept
{
  return CreateUnique<X11WindowsBackend>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_X11
