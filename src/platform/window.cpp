#include "gecko/platform/window.h"

#include <deque>
#include <unordered_map>

#include "categories.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"
#include "window_backend.h"

namespace gecko::platform {

namespace {
struct WindowState {
  WindowDesc Desc{};
  Extent2D ClientSize{};
  bool Alive{true};
};
} // namespace

namespace {
class NullWindowBackend final : public IWindowBackend {
public:
  bool CreateWindow(const WindowDesc &desc,
                    WindowHandle &outWindow) noexcept override {
    GECKO_PROF_FUNC(categories::General);

    const u64 id = ++m_NextId;
    outWindow = WindowHandle{id};

    WindowState st;
    st.Desc = desc;
    st.ClientSize = desc.Size;
    st.Alive = true;

    m_Windows.emplace(id, st);

    GECKO_INFO(categories::General, "Created null window id=%llu\n",
               static_cast<unsigned long long>(id));
    return true;
  }

  void DestroyWindow(WindowHandle window) noexcept override {
    GECKO_PROF_FUNC(categories::General);
    if (!window.IsValid())
      return;

    auto it = m_Windows.find(window.Id);
    if (it == m_Windows.end())
      return;

    it->second.Alive = false;

    WindowEvent ev{};
    ev.Kind = WindowEventKind::Closed;
    ev.Window = window;
    ev.TimeNs = NowNsSafe();
    m_Events.push_back(ev);

    m_Windows.erase(it);
  }

  bool IsWindowAlive(WindowHandle window) const noexcept override {
    if (!window.IsValid())
      return false;
    return m_Windows.find(window.Id) != m_Windows.end();
  }

  bool RequestClose(WindowHandle window) noexcept override {
    GECKO_PROF_FUNC(categories::General);
    if (!window.IsValid())
      return false;
    if (!IsWindowAlive(window))
      return false;

    WindowEvent ev{};
    ev.Kind = WindowEventKind::CloseRequested;
    ev.Window = window;
    ev.TimeNs = NowNsSafe();
    m_Events.push_back(ev);
    return true;
  }

  void PumpEvents() noexcept override {
    GECKO_PROF_FUNC(categories::General);
    // Null backend: no OS events.
  }

  bool PollEvent(WindowEvent &outEvent) noexcept override {
    if (m_Events.empty())
      return false;
    outEvent = m_Events.front();
    m_Events.pop_front();
    return true;
  }

  Extent2D GetClientSize(WindowHandle window) const noexcept override {
    auto it = m_Windows.find(window.Id);
    if (it == m_Windows.end())
      return Extent2D{};
    return it->second.ClientSize;
  }

  void SetTitle(WindowHandle window, const char *title) noexcept override {
    auto it = m_Windows.find(window.Id);
    if (it == m_Windows.end())
      return;
    it->second.Desc.Title = title;
  }

  DpiInfo GetDpi(WindowHandle window) const noexcept override {
    (void)window;
    return DpiInfo{};
  }

  NativeWindowHandle
  GetNativeWindowHandle(WindowHandle window) const noexcept override {
    (void)window;
    return NativeWindowHandle{};
  }

private:
  static u64 NowNsSafe() noexcept {
    if (auto *profiler = GetProfiler())
      return profiler->NowNs();
    return 0;
  }

  u64 m_NextId{0};
  std::unordered_map<u64, WindowState> m_Windows;
  std::deque<WindowEvent> m_Events;
};
} // namespace

IWindowBackend &GetNullWindowBackend() noexcept {
  static NullWindowBackend backend;
  return backend;
}

// Provided by src/platform/linux/x11_window_backend.cpp when enabled.
#if defined(__linux__) && defined(GECKO_PLATFORM_LINUX_X11)
IWindowBackend &GetXlibWindowBackend() noexcept;
#endif

IWindowBackend &ResolveWindowBackend(WindowBackendKind requested) noexcept {
  auto &nullBackend = GetNullWindowBackend();

#if defined(__linux__) && defined(GECKO_PLATFORM_LINUX_X11)
  constexpr bool hasX11 = true;
#else
  constexpr bool hasX11 = false;
#endif

#if defined(_WIN32)
  constexpr bool hasWin32 = false; // not implemented yet
#else
  constexpr bool hasWin32 = false;
#endif

#if defined(__APPLE__)
  constexpr bool hasCocoa = false; // not implemented yet
#else
  constexpr bool hasCocoa = false;
#endif

  switch (requested) {
  case WindowBackendKind::Null:
    return nullBackend;

  case WindowBackendKind::Xlib:
    if (hasX11) {
#if defined(__linux__) && defined(GECKO_PLATFORM_LINUX_X11)
      return GetXlibWindowBackend();
#endif
    }
    GECKO_WARN(categories::General, "Requested Xlib window backend, but it's "
                                    "not available; using Null backend\n");
    return nullBackend;

  case WindowBackendKind::Wayland:
    GECKO_WARN(categories::General,
               "Requested Wayland window backend, but it's not implemented "
               "yet; using Null backend\n");
    return nullBackend;

  case WindowBackendKind::Win32:
    if (hasWin32) {
      return nullBackend;
    }
    GECKO_WARN(categories::General,
               "Requested Win32 window backend, but it's not implemented yet; "
               "using Null backend\n");
    return nullBackend;

  case WindowBackendKind::Cocoa:
    if (hasCocoa) {
      return nullBackend;
    }
    GECKO_WARN(categories::General,
               "Requested Cocoa window backend, but it's not implemented yet; "
               "using Null backend\n");
    return nullBackend;

  case WindowBackendKind::Auto:
  default:
    if (hasX11) {
#if defined(__linux__) && defined(GECKO_PLATFORM_LINUX_X11)
      return GetXlibWindowBackend();
#endif
    }
    return nullBackend;
  }
}

} // namespace gecko::platform
