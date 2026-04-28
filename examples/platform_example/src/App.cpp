#include "App.h"

#include <cstdio>
#include <gecko/core/labels.h>
#include <gecko/core/services.h>
#include <gecko/core/services/log.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>
#include <gecko/platform/platform_events.h>
#include <gecko/platform/windows_interface.h>

namespace gecko::examples::platform_example {

namespace {

constexpr ::gecko::Label App_Label = ::gecko::MakeLabel("app.platform_example");
constexpr ::gecko::Label Main_Label =
    ::gecko::MakeLabel("app.platform_example.main");

const char* WindowModeToString(::gecko::platform::WindowMode mode) noexcept
{
  using ::gecko::platform::WindowMode;
  switch (mode)
  {
  case WindowMode::Windowed:
    return "Windowed";
  case WindowMode::Fullscreen:
    return "Fullscreen";
  case WindowMode::BorderlessFullscreen:
    return "BorderlessFullscreen";
  }
  return "Unknown";
}

const char* WindowStateToString(::gecko::platform::WindowState state) noexcept
{
  using ::gecko::platform::WindowState;
  switch (state)
  {
  case WindowState::Normal:
    return "Normal";
  case WindowState::Minimized:
    return "Minimized";
  case WindowState::Maximized:
    return "Maximized";
  case WindowState::Hidden:
    return "Hidden";
  }
  return "Unknown";
}

const char* BoolStr(bool v) noexcept
{
  return v ? "true" : "false";
}

::gecko::platform::IWindowsBackend& Windows() noexcept
{
  return *::gecko::platform::GetWindows();
}

}  // namespace

App::AllocatorInstaller::AllocatorInstaller(::gecko::IAllocator* a) noexcept
    : Ok(::gecko::SetAllocator(a))
{}

App::AllocatorInstaller::~AllocatorInstaller()
{
  ::gecko::ResetAllocator();
}

::gecko::Label App::ExampleModule::RootLabel() const noexcept
{
  return App_Label;
}

bool App::ExampleModule::Startup(::gecko::IModuleRegistry&) noexcept
{
  return true;
}

void App::ExampleModule::Shutdown(::gecko::IModuleRegistry&) noexcept
{}

App::App() : m_RuntimeModule(m_JobSystem, m_Profiler, m_Logger, m_EventBus)
{
  if (!m_AllocatorInstaller.Ok)
    return;

  m_JobSystem.SetWorkerThreadCount(4);
  m_Engine = ::gecko::Engine::Create(
      {&m_RuntimeModule, &m_PlatformModule, &m_AppModule});
  if (!m_Engine)
    return;

  ::gecko::SetThreadProfilerName("main");
  AttachSinks();
  GECKO_INFO(Main_Label, ::gecko::VersionFullString());

  CreateMainWindow();
  if (m_MainWindow.IsValid())
  {
    SubscribeWindowEvents();
    PrintHelp();
  }
}

App::~App()
{
  for (auto& h : m_SpawnedWindows)
  {
    if (Windows().IsWindowAlive(h))
      Windows().DestroyWindow(h);
  }
  m_SpawnedWindows.clear();

  if (m_MainWindow.IsValid() && Windows().IsWindowAlive(m_MainWindow))
    Windows().DestroyWindow(m_MainWindow);

  if (m_SinksAttached)
    DetachSinks();
  m_Engine.reset();
}

void App::AttachSinks()
{
  if (auto* logger = ::gecko::GetLogger())
  {
    m_FileSink.RegisterWith(logger);
    m_ConsoleSink.RegisterWith(logger);
    logger->SetLevel(::gecko::LogLevel::Info);
  }
  m_SinksAttached = true;
}

void App::DetachSinks()
{
  m_ConsoleSink.Unregister();
  m_FileSink.Unregister();
  m_SinksAttached = false;
}

void App::CreateMainWindow()
{
  using ::gecko::platform::WindowDesc;
  using ::gecko::platform::WindowMode;

  WindowDesc desc;
  desc.Title = "Gecko Platform Example";
  desc.Size = {1280, 720};
  desc.Visible = true;
  desc.Resizable = true;
  desc.Mode = WindowMode::Windowed;

  m_MainWindow = Windows().CreateWindow(desc);
  if (!m_MainWindow.IsValid())
  {
    GECKO_ERROR(Main_Label, "Failed to create main window");
    return;
  }

  GECKO_INFO(Main_Label, "Main window created");
  PrintWindowInfo();
}

void App::SubscribeWindowEvents()
{
  using namespace ::gecko::platform;

  m_CloseSub = ::gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView view) {
        auto* self = static_cast<App*>(user);
        const auto* p = static_cast<const events::WindowCloseRequestedPayload*>(
            view.Data());
        if (p->Window == self->m_MainWindow)
        {
          self->m_Running = false;
          return;
        }
        for (auto it = self->m_SpawnedWindows.begin();
             it != self->m_SpawnedWindows.end(); ++it)
        {
          if (*it == p->Window)
          {
            GECKO_INFO(Main_Label, "Child window %llu closed",
                       static_cast<unsigned long long>(it->Id));
            Windows().DestroyWindow(*it);
            self->m_SpawnedWindows.erase(it);
            break;
          }
        }
      },
      this);

  m_KeySub = ::gecko::SubscribeEvent(
      events::WindowKey,
      [](void* user, const ::gecko::EventMeta&, ::gecko::EventView view) {
        auto* self = static_cast<App*>(user);
        const auto* p =
            static_cast<const events::WindowKeyPayload*>(view.Data());
        if (!p->Down || p->Repeat)
          return;
        self->OnKey(p->Key);
      },
      this);

  m_FocusSub = ::gecko::SubscribeEvent(
      events::WindowFocusChanged,
      [](void*, const ::gecko::EventMeta&, ::gecko::EventView view) {
        const auto* p =
            static_cast<const events::WindowFocusChangedPayload*>(view.Data());
        GECKO_INFO(Main_Label, "WindowFocus: id=%llu focused=%u",
                   static_cast<unsigned long long>(p->Window.Id),
                   static_cast<unsigned>(p->Focused));
      },
      nullptr);

  m_MovedSub = ::gecko::SubscribeEvent(
      events::WindowMoved,
      [](void*, const ::gecko::EventMeta&, ::gecko::EventView view) {
        const auto* p =
            static_cast<const events::WindowMovedPayload*>(view.Data());
        GECKO_INFO(Main_Label, "WindowMoved: id=%llu pos=(%d, %d)",
                   static_cast<unsigned long long>(p->Window.Id), p->X, p->Y);
      },
      nullptr);

  m_ResizedSub = ::gecko::SubscribeEvent(
      events::WindowResized,
      [](void*, const ::gecko::EventMeta&, ::gecko::EventView view) {
        const auto* p =
            static_cast<const events::WindowResizedPayload*>(view.Data());
        GECKO_INFO(Main_Label, "WindowResized: id=%llu size=%ux%u",
                   static_cast<unsigned long long>(p->Window.Id), p->Width,
                   p->Height);
      },
      nullptr);

  m_StateChangedSub = ::gecko::SubscribeEvent(
      events::WindowStateChanged,
      [](void*, const ::gecko::EventMeta&, ::gecko::EventView view) {
        const auto* p =
            static_cast<const events::WindowStateChangedPayload*>(view.Data());
        GECKO_INFO(Main_Label, "WindowStateChanged: id=%llu %s -> %s",
                   static_cast<unsigned long long>(p->Window.Id),
                   WindowStateToString(p->OldState),
                   WindowStateToString(p->NewState));
      },
      nullptr);
}

int App::Run()
{
  if (!IsValid())
    return 1;

  // Frame callback is also used during Win32 modal drag/resize so the
  // app keeps ticking.
  ::gecko::platform::SetModalFrameCallback(
      [](void* ud) { static_cast<App*>(ud)->RunFrame(); }, this);

  GECKO_INFO(Main_Label, "Entering main loop...");
  while (m_Running && Windows().IsWindowAlive(m_MainWindow))
  {
    GECKO_SCOPE_NAMED(Main_Label, "MainLoop");
    {
      GECKO_SCOPE_NAMED(Main_Label, "PumpEvents");
      ::gecko::platform::PumpEvents();
    }
    RunFrame();
    GECKO_FRAME(Main_Label, "Frame");
  }
  return 0;
}

void App::RunFrame()
{
  GECKO_SCOPE_NAMED(Main_Label, "Frame");
  {
    GECKO_SCOPE_NAMED(Main_Label, "DispatchEvents");
    (void)::gecko::DispatchEvents();
  }
  PollInput();
  GECKO_PRECISE_SLEEP_NS(16'000'000);
}

void App::PollInput()
{
  using ::gecko::platform::MouseButton;
  auto* input = ::gecko::platform::GetInput();
  if (!input)
    return;

  static constexpr struct
  {
    MouseButton Btn;
    const char* Name;
  } kButtons[] = {{MouseButton::Left, "Left"},
                  {MouseButton::Right, "Right"},
                  {MouseButton::Middle, "Middle"}};
  for (const auto& b : kButtons)
  {
    if (input->WasMouseButtonPressed(b.Btn))
    {
      auto p = input->GetMousePosition();
      GECKO_INFO(Main_Label, "Input: %s mouse pressed at (%d, %d)", b.Name, p.X,
                 p.Y);
    }
    if (input->WasMouseButtonReleased(b.Btn))
    {
      auto p = input->GetMousePosition();
      GECKO_INFO(Main_Label, "Input: %s mouse released at (%d, %d)", b.Name,
                 p.X, p.Y);
    }
  }

  if (const float scrollY = input->GetMouseScrollY(); scrollY != 0.0f)
    GECKO_INFO(Main_Label, "Input: scrollY=%.2f", static_cast<double>(scrollY));

  const auto focused = input->FocusedWindow();
  const auto hovered = input->HoveredWindow();
  if (!m_InputWatch.Initialized)
  {
    m_InputWatch.LastFocused = focused;
    m_InputWatch.LastHovered = hovered;
    m_InputWatch.Initialized = true;
    return;
  }
  if (focused.Id != m_InputWatch.LastFocused.Id)
  {
    GECKO_INFO(Main_Label, "Input: focused window %llu -> %llu",
               static_cast<unsigned long long>(m_InputWatch.LastFocused.Id),
               static_cast<unsigned long long>(focused.Id));
    m_InputWatch.LastFocused = focused;
  }
  if (hovered.Id != m_InputWatch.LastHovered.Id)
  {
    GECKO_INFO(Main_Label, "Input: hovered window %llu -> %llu",
               static_cast<unsigned long long>(m_InputWatch.LastHovered.Id),
               static_cast<unsigned long long>(hovered.Id));
    m_InputWatch.LastHovered = hovered;
  }
}

void App::OnKey(::gecko::platform::KeyCode key)
{
  using ::gecko::platform::KeyCode;
  switch (key)
  {
  case KeyCode::D1:
  case KeyCode::D2:
  case KeyCode::D3:
  case KeyCode::D4:
  case KeyCode::W:
    HandleSpawnKey(key);
    return;

  case KeyCode::D:
  case KeyCode::R:
  case KeyCode::F:
  case KeyCode::T:
  case KeyCode::M:
    HandleMainWindowKey(key);
    return;

  case KeyCode::D5:
  case KeyCode::D6:
  case KeyCode::D7:
  case KeyCode::D8:
    HandleTitleBarButtonsKey(key);
    return;

  case KeyCode::D9:
  case KeyCode::D0:
    HandleSizeConstraintsKey(key);
    return;

  case KeyCode::H:
    PrintHelp();
    return;
  case KeyCode::I:
    PrintWindowInfo();
    return;
  case KeyCode::P:
    PrintInputSnapshot();
    return;
  case KeyCode::Escape:
    m_Running = false;
    return;
  default:
    return;
  }
}

void App::HandleSpawnKey(::gecko::platform::KeyCode key)
{
  using namespace ::gecko::platform;
  WindowDesc desc;
  desc.Mode = WindowMode::Windowed;

  switch (key)
  {
  case KeyCode::D1:
    desc.Title = "Gecko - Small Fixed";
    desc.Size = {400, 300};
    desc.Resizable = false;
    break;
  case KeyCode::D2:
    desc.Title = "Gecko - Large Resizable";
    desc.Size = {1600, 900};
    desc.Resizable = true;
    break;
  case KeyCode::D3:
    desc.Title = "Gecko - Borderless";
    desc.Size = {800, 600};
    desc.Decorated = false;
    break;
  case KeyCode::D4:
    desc.Title = "Gecko - Fullscreen";
    desc.Mode = WindowMode::BorderlessFullscreen;
    break;
  case KeyCode::W:
    if (m_SpawnedWindows.empty())
    {
      GECKO_INFO(Main_Label, "No spawned windows to close");
      return;
    }
    {
      WindowHandle last = m_SpawnedWindows.back();
      GECKO_INFO(Main_Label, "Closing last spawned window (id=%llu)",
                 static_cast<unsigned long long>(last.Id));
      Windows().DestroyWindow(last);
      m_SpawnedWindows.pop_back();
    }
    return;
  default:
    return;
  }

  WindowHandle h = Windows().CreateWindow(desc);
  if (h.IsValid())
  {
    m_SpawnedWindows.push_back(h);
    GECKO_INFO(Main_Label, "Spawned '%s' (id=%llu)", desc.Title,
               static_cast<unsigned long long>(h.Id));
  }
}

void App::HandleMainWindowKey(::gecko::platform::KeyCode key)
{
  using namespace ::gecko::platform;
  auto& windows = Windows();
  const WindowHandle main = m_MainWindow;

  switch (key)
  {
  case KeyCode::D: {
    const bool decorated = windows.IsDecorated(main);
    windows.SetDecorated(main, !decorated);
    GECKO_INFO(Main_Label, "Decorations: %s -> %s", BoolStr(decorated),
               BoolStr(!decorated));
    return;
  }
  case KeyCode::R: {
    const bool resizable = windows.IsResizable(main);
    windows.SetResizable(main, !resizable);
    GECKO_INFO(Main_Label, "Resizable: %s -> %s", BoolStr(resizable),
               BoolStr(!resizable));
    return;
  }
  case KeyCode::F: {
    const WindowMode mode = windows.GetWindowMode(main);
    const WindowMode next = (mode == WindowMode::Windowed)
                                ? WindowMode::BorderlessFullscreen
                                : WindowMode::Windowed;
    windows.SetWindowMode(main, next);
    GECKO_INFO(Main_Label, "Window mode: %s -> %s", WindowModeToString(mode),
               WindowModeToString(next));
    return;
  }
  case KeyCode::T: {
    const bool topmost = windows.IsAlwaysOnTop(main);
    windows.SetAlwaysOnTop(main, !topmost);
    GECKO_INFO(Main_Label, "Always on top: %s -> %s", BoolStr(topmost),
               BoolStr(!topmost));
    return;
  }
  case KeyCode::M: {
    const WindowState st = windows.GetWindowState(main);
    WindowState next;
    switch (st)
    {
    case WindowState::Normal:
      next = WindowState::Maximized;
      break;
    case WindowState::Maximized:
      next = WindowState::Minimized;
      break;
    default:
      next = WindowState::Normal;
      break;
    }
    windows.SetWindowState(main, next);
    GECKO_INFO(Main_Label, "Window state: %s -> %s", WindowStateToString(st),
               WindowStateToString(next));
    return;
  }
  default:
    return;
  }
}

void App::HandleTitleBarButtonsKey(::gecko::platform::KeyCode key)
{
  using namespace ::gecko::platform;
  auto& windows = Windows();
  const WindowHandle main = m_MainWindow;
  const WindowButtons btns = windows.GetWindowButtons(main);

  switch (key)
  {
  case KeyCode::D5: {
    const WindowButtons next = Any(btns & WindowButtons::Close)
                                   ? (btns & ~WindowButtons::Close)
                                   : (btns | WindowButtons::Close);
    windows.SetWindowButtons(main, next);
    GECKO_INFO(Main_Label, "Close button: %s",
               BoolStr(Any(next & WindowButtons::Close)));
    return;
  }
  case KeyCode::D6: {
    const WindowButtons next = Any(btns & WindowButtons::Minimize)
                                   ? (btns & ~WindowButtons::Minimize)
                                   : (btns | WindowButtons::Minimize);
    windows.SetWindowButtons(main, next);
    GECKO_INFO(Main_Label, "Minimize button: %s",
               BoolStr(Any(next & WindowButtons::Minimize)));
    return;
  }
  case KeyCode::D7: {
    const WindowButtons next = Any(btns & WindowButtons::Maximize)
                                   ? (btns & ~WindowButtons::Maximize)
                                   : (btns | WindowButtons::Maximize);
    windows.SetWindowButtons(main, next);
    GECKO_INFO(Main_Label, "Maximize button: %s",
               BoolStr(Any(next & WindowButtons::Maximize)));
    return;
  }
  case KeyCode::D8:
    windows.SetWindowButtons(main, WindowButtons::All);
    GECKO_INFO(Main_Label, "Restored all title-bar buttons");
    return;
  default:
    return;
  }
}

void App::HandleSizeConstraintsKey(::gecko::platform::KeyCode key)
{
  using namespace ::gecko::platform;
  auto& windows = Windows();
  if (key == KeyCode::D9)
  {
    windows.SetMinSize(m_MainWindow, {400, 300});
    windows.SetMaxSize(m_MainWindow, {1920, 1080});
    GECKO_INFO(Main_Label, "Size constraints: min=400x300, max=1920x1080");
  }
  else if (key == KeyCode::D0)
  {
    windows.SetMinSize(m_MainWindow, {0, 0});
    windows.SetMaxSize(m_MainWindow, {0, 0});
    GECKO_INFO(Main_Label, "Size constraints cleared");
  }
}

void App::PrintHelp()
{
  GECKO_INFO(Main_Label, "===== Gecko Platform Example - Window API =====");
  GECKO_INFO(Main_Label, "  Window Creation:");
  GECKO_INFO(Main_Label, "    [1]  small fixed (400x300)");
  GECKO_INFO(Main_Label, "    [2]  large resizable (1600x900)");
  GECKO_INFO(Main_Label, "    [3]  borderless (800x600)");
  GECKO_INFO(Main_Label, "    [4]  borderless fullscreen");
  GECKO_INFO(Main_Label, "    [W]  close last spawned window");
  GECKO_INFO(Main_Label, "  Main Window:");
  GECKO_INFO(Main_Label, "    [D]  toggle decorations");
  GECKO_INFO(Main_Label, "    [R]  toggle resizable");
  GECKO_INFO(Main_Label, "    [F]  toggle borderless fullscreen");
  GECKO_INFO(Main_Label, "    [T]  toggle always-on-top");
  GECKO_INFO(Main_Label, "    [M]  cycle Normal/Maximized/Minimized");
  GECKO_INFO(Main_Label, "  Title-bar buttons:");
  GECKO_INFO(Main_Label, "    [5]/[6]/[7] toggle close/min/max");
  GECKO_INFO(Main_Label, "    [8]  restore all buttons");
  GECKO_INFO(Main_Label, "  Size constraints:");
  GECKO_INFO(Main_Label, "    [9]  set min 400x300, max 1920x1080");
  GECKO_INFO(Main_Label, "    [0]  clear constraints");
  GECKO_INFO(Main_Label, "  Other:");
  GECKO_INFO(Main_Label, "    [H]  print this help");
  GECKO_INFO(Main_Label, "    [I]  print window info");
  GECKO_INFO(Main_Label, "    [P]  print input snapshot");
  GECKO_INFO(Main_Label, "    [Esc] quit");
  GECKO_INFO(Main_Label, "===============================================");
}

void App::PrintWindowInfo()
{
  using namespace ::gecko::platform;
  auto& windows = Windows();
  const WindowHandle main = m_MainWindow;

  const DpiInfo dpi = windows.GetDpi(main);
  const Extent2D size = windows.GetClientSize(main);
  const ::gecko::math::Int2 pos = windows.GetPosition(main);
  const NativeWindowHandle native = windows.GetNativeWindowHandle(main);
  const WindowButtons btns = windows.GetWindowButtons(main);

  GECKO_INFO(Main_Label, "----- Window Info -----");
  GECKO_INFO(Main_Label, "  Size:          %ux%u", size.Width, size.Height);
  GECKO_INFO(Main_Label, "  Position:      (%d, %d)", pos.X, pos.Y);
  GECKO_INFO(Main_Label, "  DPI:           %u (scale %.2f)", dpi.Dpi,
             static_cast<double>(dpi.Scale));
  GECKO_INFO(Main_Label, "  Native:        backend=%u handle=%p",
             static_cast<unsigned>(native.Backend), native.Handle);
  GECKO_INFO(Main_Label, "  Mode:          %s",
             WindowModeToString(windows.GetWindowMode(main)));
  GECKO_INFO(Main_Label, "  State:         %s",
             WindowStateToString(windows.GetWindowState(main)));
  GECKO_INFO(Main_Label, "  Decorated:     %s",
             BoolStr(windows.IsDecorated(main)));
  GECKO_INFO(Main_Label, "  Resizable:     %s",
             BoolStr(windows.IsResizable(main)));
  GECKO_INFO(Main_Label, "  Always on top: %s",
             BoolStr(windows.IsAlwaysOnTop(main)));
  GECKO_INFO(Main_Label, "  Buttons:       close=%s min=%s max=%s",
             BoolStr(Any(btns & WindowButtons::Close)),
             BoolStr(Any(btns & WindowButtons::Minimize)),
             BoolStr(Any(btns & WindowButtons::Maximize)));
}

void App::PrintInputSnapshot()
{
  using ::gecko::platform::KeyCode;
  using ::gecko::platform::MouseButton;
  auto* input = ::gecko::platform::GetInput();
  if (!input)
    return;

  const auto pos = input->GetMousePosition();
  const auto delta = input->GetMouseDelta();
  const auto focused = input->FocusedWindow();
  const auto hovered = input->HoveredWindow();
  const bool shift = input->IsKeyDown(KeyCode::LeftShift) ||
                     input->IsKeyDown(KeyCode::RightShift);
  const bool ctrl = input->IsKeyDown(KeyCode::LeftControl) ||
                    input->IsKeyDown(KeyCode::RightControl);

  GECKO_INFO(Main_Label, "----- Input snapshot -----");
  GECKO_INFO(Main_Label, "  Mouse:    pos=(%d, %d) delta=(%d, %d) scrollY=%.2f",
             pos.X, pos.Y, delta.X, delta.Y,
             static_cast<double>(input->GetMouseScrollY()));
  GECKO_INFO(Main_Label, "  Buttons:  L=%s R=%s M=%s",
             input->IsMouseButtonDown(MouseButton::Left) ? "down" : "up",
             input->IsMouseButtonDown(MouseButton::Right) ? "down" : "up",
             input->IsMouseButtonDown(MouseButton::Middle) ? "down" : "up");
  GECKO_INFO(Main_Label, "  Modifiers: shift=%s ctrl=%s", BoolStr(shift),
             BoolStr(ctrl));
  GECKO_INFO(Main_Label, "  Focused=%llu Hovered=%llu",
             static_cast<unsigned long long>(focused.Id),
             static_cast<unsigned long long>(hovered.Id));
}

}  // namespace gecko::examples::platform_example
