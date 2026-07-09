#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/platform/input.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/platform_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace gecko;
using namespace gecko::platform;

namespace {

struct TestServiceScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  runtime::EventBus events;
  runtime::RuntimeModule runtimeMod;
  PlatformModule platformMod;
  ::gecko::EngineResult engine;

  static PlatformConfig MakeNullConfig() noexcept
  {
    PlatformConfig cfg;
    cfg.Backend = DisplayBackendKind::Null;
    return cfg;
  }

  TestServiceScope() : runtimeMod(jobs, profiler, logger, events), platformMod(MakeNullConfig())
  {
    REQUIRE(SetAllocator(&alloc));
    ::gecko::IModule* modules[] = {&runtimeMod, &platformMod};
    engine = ::gecko::Engine::Create(modules);
    REQUIRE(engine.has_value());
  }

  ~TestServiceScope()
  {
    engine.reset();
    ResetAllocator();
  }
};

}  // namespace

TEST_CASE("Null backend: create and destroy window", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});
  REQUIRE(win.IsValid());
  REQUIRE(GetWindows()->IsWindowAlive(win));

  GetWindows()->DestroyWindow(win);
  REQUIRE_FALSE(GetWindows()->IsWindowAlive(win));
}

TEST_CASE("Null backend: client size matches desc", "[platform][context]")
{
  TestServiceScope scope;

  WindowDesc desc;
  desc.Size = {800, 600};
  WindowHandle win = GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());

  Extent2D size = GetWindows()->GetClientSize(win);
  REQUIRE(size.Width == 800);
  REQUIRE(size.Height == 600);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: set title doesn't crash", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});
  GetWindows()->SetTitle(win, "Test Title");
  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: request close enqueues event", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});
  REQUIRE(GetWindows()->RequestClose(win));

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const gecko::EventMeta&, gecko::EventView) { (*static_cast<int*>(user))++; }, &received);

  PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: multiple windows", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle w1 = GetWindows()->CreateWindow({});
  WindowHandle w2 = GetWindows()->CreateWindow({});
  WindowHandle w3 = GetWindows()->CreateWindow({});
  REQUIRE(w1.IsValid());
  REQUIRE(w2.IsValid());
  REQUIRE(w3.IsValid());

  REQUIRE(w1 != w2);
  REQUIRE(w2 != w3);

  REQUIRE(GetWindows()->IsWindowAlive(w1));
  REQUIRE(GetWindows()->IsWindowAlive(w2));
  REQUIRE(GetWindows()->IsWindowAlive(w3));

  GetWindows()->DestroyWindow(w2);
  REQUIRE(GetWindows()->IsWindowAlive(w1));
  REQUIRE_FALSE(GetWindows()->IsWindowAlive(w2));
  REQUIRE(GetWindows()->IsWindowAlive(w3));

  GetWindows()->DestroyWindow(w1);
  GetWindows()->DestroyWindow(w3);
}

TEST_CASE("Null backend: DPI info has defaults", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  DpiInfo dpi = GetWindows()->GetDpi(win);
  REQUIRE(dpi.Dpi > 0);
  REQUIRE(dpi.Scale > 0.0f);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: PumpEvents doesn't crash", "[platform][context]")
{
  TestServiceScope scope;

  PumpEvents();
}

TEST_CASE("Null backend: invalid window operations are safe", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle invalid;
  REQUIRE_FALSE(GetWindows()->IsWindowAlive(invalid));
  REQUIRE_FALSE(GetWindows()->RequestClose(invalid));
  GetWindows()->DestroyWindow(invalid);
}

TEST_CASE("Null backend: set and get client size", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  GetWindows()->SetClientSize(win, {1024, 768});
  Extent2D size = GetWindows()->GetClientSize(win);
  REQUIRE(size.Width == 1024);
  REQUIRE(size.Height == 768);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: get title returns desc title", "[platform][context]")
{
  TestServiceScope scope;

  WindowDesc desc;
  desc.Title = "My Window";
  WindowHandle win = GetWindows()->CreateWindow(desc);

  const char* title = GetWindows()->GetTitle(win);
  REQUIRE(::std::strcmp(title, "My Window") == 0);

  GetWindows()->SetTitle(win, "Updated");
  REQUIRE(::std::strcmp(GetWindows()->GetTitle(win), "Updated") == 0);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: set and get position", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  GetWindows()->SetPosition(win, {100, 200});
  auto pos = GetWindows()->GetPosition(win);
  REQUIRE(pos.X == 100);
  REQUIRE(pos.Y == 200);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: set position fires WindowMoved event", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowMoved, [](void* user, const gecko::EventMeta&, gecko::EventView) { (*static_cast<int*>(user))++; },
      &received);

  GetWindows()->SetPosition(win, {50, 75});
  PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: set and get window state", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  REQUIRE(GetWindows()->GetWindowState(win) == WindowState::Normal);

  GetWindows()->SetWindowState(win, WindowState::Minimized);
  REQUIRE(GetWindows()->GetWindowState(win) == WindowState::Minimized);

  GetWindows()->SetWindowState(win, WindowState::Maximized);
  REQUIRE(GetWindows()->GetWindowState(win) == WindowState::Maximized);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: set state fires WindowStateChanged event", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowStateChanged,
      [](void* user, const gecko::EventMeta&, gecko::EventView) { (*static_cast<int*>(user))++; }, &received);

  GetWindows()->SetWindowState(win, WindowState::Hidden);
  PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: decorated get/set", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  REQUIRE(GetWindows()->IsDecorated(win) == true);

  GetWindows()->SetDecorated(win, false);
  REQUIRE(GetWindows()->IsDecorated(win) == false);

  GetWindows()->SetDecorated(win, true);
  REQUIRE(GetWindows()->IsDecorated(win) == true);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: cursor mode get/set", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  REQUIRE(GetWindows()->GetCursorMode(win) == CursorMode::Normal);

  GetWindows()->SetCursorMode(win, CursorMode::Hidden);
  REQUIRE(GetWindows()->GetCursorMode(win) == CursorMode::Hidden);

  GetWindows()->SetCursorMode(win, CursorMode::Locked);
  REQUIRE(GetWindows()->GetCursorMode(win) == CursorMode::Locked);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: request focus does not crash", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  GetWindows()->RequestFocus(win);

  GetWindows()->DestroyWindow(win);
}

TEST_CASE("Null backend: hidden window has Hidden state", "[platform][context]")
{
  TestServiceScope scope;

  WindowDesc desc;
  desc.Visible = false;
  WindowHandle win = GetWindows()->CreateWindow(desc);

  REQUIRE(GetWindows()->GetWindowState(win) == WindowState::Hidden);

  GetWindows()->DestroyWindow(win);
}

// ── Monitor backend tests ──────────────────────────────────────────────

TEST_CASE("Null monitor backend: enumerates one virtual monitor", "[platform][context]")
{
  TestServiceScope scope;

  REQUIRE(GetMonitors()->GetMonitorCount() == 1);
}

TEST_CASE("Null monitor backend: primary monitor exists", "[platform][context]")
{
  TestServiceScope scope;

  MonitorHandle primary = GetMonitors()->GetPrimaryMonitor();
  REQUIRE(primary.IsValid());
}

TEST_CASE("Null monitor backend: get handle by index", "[platform][context]")
{
  TestServiceScope scope;

  MonitorHandle h = GetMonitors()->GetMonitorHandle(0);
  REQUIRE(h.IsValid());

  MonitorHandle invalid = GetMonitors()->GetMonitorHandle(99);
  REQUIRE_FALSE(invalid.IsValid());
}

TEST_CASE("Null monitor backend: monitor properties are valid", "[platform][context]")
{
  TestServiceScope scope;

  MonitorHandle h = GetMonitors()->GetMonitorHandle(0);

  MonitorInfo info = GetMonitors()->GetMonitorProperties(h);
  REQUIRE(info.IsPrimary);
  REQUIRE(info.Dpi > 0);
  REQUIRE(info.RefreshRateMilliHz > 0);
  REQUIRE_FALSE(info.Bounds.IsEmpty());
}

TEST_CASE("Null monitor backend: bounds and work area", "[platform][context]")
{
  TestServiceScope scope;

  MonitorHandle h = GetMonitors()->GetMonitorHandle(0);

  MonitorBounds mb = GetMonitors()->GetMonitorBounds(h);
  REQUIRE_FALSE(mb.Bounds.IsEmpty());
  REQUIRE_FALSE(mb.WorkArea.IsEmpty());
}

TEST_CASE("Null monitor backend: invalid handle returns false", "[platform][context]")
{
  TestServiceScope scope;

  MonitorHandle invalid;
  MonitorInfo info = GetMonitors()->GetMonitorProperties(invalid);
  REQUIRE(info.Bounds.IsEmpty());

  MonitorBounds mb = GetMonitors()->GetMonitorBounds(invalid);
  REQUIRE(mb.Bounds.IsEmpty());
  REQUIRE(mb.WorkArea.IsEmpty());
}

// ── Event delivery tests ───────────────────────────────────────────────

TEST_CASE("Null backend: destroy window enqueues WindowClosed event", "[platform][context]")
{
  TestServiceScope scope;

  WindowHandle win = GetWindows()->CreateWindow({});

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowClosed, [](void* user, const gecko::EventMeta&, gecko::EventView) { (*static_cast<int*>(user))++; },
      &received);

  GetWindows()->DestroyWindow(win);
  PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);
}

// ── Config resolution tests ────────────────────────────────────────────

TEST_CASE("Resolve: explicit backend is preserved", "[platform][config]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  PlatformConfig resolved = Resolve(cfg);
  REQUIRE(resolved.Backend == DisplayBackendKind::Null);
}

TEST_CASE("Resolve: Auto resolves to concrete backend", "[platform][config]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Auto;
  PlatformConfig resolved = Resolve(cfg);
  REQUIRE(resolved.Backend != DisplayBackendKind::Auto);
}

TEST_CASE("PlatformModule stores resolved config", "[platform][context]")
{
  TestServiceScope scope;

  REQUIRE(scope.platformMod.Config().Backend == DisplayBackendKind::Null);
}

// --- Caller-owned backend injection ---------------------------------
//
// PlatformModule::Backends takes raw pointers (caller-owned). When a
// pointer is non-null the module must publish *that* exact instance as
// the service, instead of constructing its own default. Verifies the
// "user owns the memory" rule documented in MODULE_API_SHAPING.md.

TEST_CASE("PlatformModule publishes injected backends", "[platform][context][injection]")
{
  PlatformConfig cfg;
  cfg.Backend = DisplayBackendKind::Null;

  // Caller-owned backends. Held in Unique<> here purely so the test
  // doesn't have to know the concrete null types — but the *module*
  // sees raw pointers and never takes ownership.
  ::gecko::Unique<IWindowsBackend> windows = IWindowsBackend::Create(cfg);
  ::gecko::Unique<IMonitorsBackend> monitors = IMonitorsBackend::Create(cfg);
  REQUIRE(windows);
  REQUIRE(monitors);

  IWindowsBackend* windowsRaw = windows.get();
  IMonitorsBackend* monitorsRaw = monitors.get();

  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  runtime::EventBus events;
  runtime::RuntimeModule runtimeMod {jobs, profiler, logger, events};

  PlatformModule platformMod {cfg, PlatformModule::Backends {
                                       .Windows = windowsRaw,
                                       .Monitors = monitorsRaw,
                                   }};

  REQUIRE(SetAllocator(&alloc));
  ::gecko::IModule* modules[] = {&runtimeMod, &platformMod};
  auto engine = ::gecko::Engine::Create(modules);
  REQUIRE(engine.has_value());

  // Injected pointers must be the ones published as services.
  REQUIRE(GetWindows() == windowsRaw);
  REQUIRE(GetMonitors() == monitorsRaw);
  // Input was left null, so the module should have constructed a
  // default — accessor must still be non-null and not equal to either
  // injected pointer.
  REQUIRE(GetInput() != nullptr);

  engine.reset();
  ResetAllocator();

  // Caller still owns the backends after Shutdown.
  REQUIRE(windows);
  REQUIRE(monitors);
}
