/// @file
/// Feature test for the Graphics module + service stack.
///
/// Boots the full Engine with `RuntimeModule`, `PlatformModule`, and
/// `GraphicsModule` (configured for `NullDevice` so the test stays
/// headless). Verifies module ordering, service publication, and
/// sampler attachment to a profiler.
///
/// `NullDevice` is intentionally chosen for the feature test so this
/// runs on CI without a GPU. The `Backends` injection path covers the
/// "swap in a real device for tests" case.

#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/platform/platform_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/immediate_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/runtime_module.h"
#include "gecko/runtime/thread_pool_job_system.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::graphics;

namespace {

struct FullStack
{
  SystemAllocator alloc;
  ::gecko::runtime::ThreadPoolJobSystem jobs;
  ::gecko::runtime::RingProfiler profiler {1 << 16};
  ::gecko::runtime::ImmediateLogger logger;
  ::gecko::runtime::EventBus events;
  ::gecko::runtime::RuntimeModule runtime {jobs, profiler, logger, events};
  ::gecko::platform::PlatformModule platform {};
  GraphicsModule graphics {GraphicsConfig {}};
  EngineResult engine;

  FullStack()
  {
    REQUIRE(SetAllocator(&alloc));
    IModule* modules[] = {&runtime, &platform, &graphics};
    engine = Engine::Create(modules);
    REQUIRE(engine.has_value());
  }
  ~FullStack()
  {
    engine.reset();
    ResetAllocator();
  }
};

}  // namespace

TEST_CASE("Full stack: GraphicsModule comes up after Runtime + Platform", "[feature][graphics][module]")
{
  FullStack stack;

  // All four foundational services must be live.
  REQUIRE(GetJobSystem() != nullptr);
  REQUIRE(GetProfiler() != nullptr);
  REQUIRE(GetLogger() != nullptr);
  REQUIRE(GetEventBus() != nullptr);

  // GraphicsModule must publish a device.
  REQUIRE(GetGraphicsDevice() != nullptr);
  REQUIRE(stack.engine->Modules().Service<GraphicsDevice>() == GetGraphicsDevice());
}

TEST_CASE("Full stack: NullDevice does not publish a GPU sampler", "[feature][graphics][module]")
{
  FullStack stack;

  // NullDevice has no timestamp queries -- sampler service stays unpublished.
  REQUIRE(GetGpuSampler() == nullptr);
  REQUIRE(stack.engine->Modules().Service<IGpuSampler>() == nullptr);
}

TEST_CASE("Full stack: GraphicsModule shutdown order is clean", "[feature][graphics][module]")
{
  FullStack stack;
  REQUIRE(GetGraphicsDevice() != nullptr);

  // Tearing down the engine must unpublish everything in reverse order
  // and leave the accessors null.
  stack.engine.reset();
  REQUIRE(GetGraphicsDevice() == nullptr);
  REQUIRE(GetGpuSampler() == nullptr);
}

TEST_CASE("Full stack: GraphicsModule honours Backends injection", "[feature][graphics][module]")
{
  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  ::gecko::runtime::ThreadPoolJobSystem jobs;
  ::gecko::runtime::RingProfiler profiler {1 << 16};
  ::gecko::runtime::ImmediateLogger logger;
  ::gecko::runtime::EventBus events;
  ::gecko::runtime::RuntimeModule runtime(jobs, profiler, logger, events);
  ::gecko::platform::PlatformModule platform {};

  auto injected = CreateGraphicsDevice();  // NullDevice owned by the test.
  REQUIRE(injected != nullptr);
  GraphicsModule graphics {GraphicsConfig {}, GraphicsModule::Backends {injected.get()}};

  IModule* modules[] = {&runtime, &platform, &graphics};
  auto engine = Engine::Create(modules);
  REQUIRE(engine.has_value());
  REQUIRE(GetGraphicsDevice() == injected.get());

  engine.reset();
  // Caller still owns the device.
  REQUIRE(injected != nullptr);
  ResetAllocator();
}

// ──────────────────────────────────────────────────────────────────
// Visible test (hidden by default; opt in with `gk test debug --visible`)
// ──────────────────────────────────────────────────────────────────
//
// Boots the full Runtime + Platform + Graphics stack with a *real*
// Vulkan device (skipped when the runtime falls back to NullDevice),
// opens a visible window, binds a swapchain, and pumps events for a
// brief moment so a human can confirm the window appears. No actual
// rendering -- the pipeline path is exercised by `graphics_example`.

#include "gecko/platform/platform_io.h"
#include "gecko/platform/window.h"
#include "gecko/platform/windows_interface.h"

#include <chrono>
#include <thread>

#if (defined(GECKO_PLATFORM_LINUX) || defined(GECKO_PLATFORM_WINDOWS)) && defined(GECKO_GRAPHICS_VULKAN)
TEST_CASE("Visible: window + Vulkan device + swapchain bind", "[.visible][feature][graphics][window]")
{
  using namespace ::gecko::platform;

  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  ::gecko::runtime::ThreadPoolJobSystem jobs;
  ::gecko::runtime::RingProfiler profiler {1 << 16};
  ::gecko::runtime::ImmediateLogger logger;
  ::gecko::runtime::EventBus events;
  ::gecko::runtime::RuntimeModule runtime(jobs, profiler, logger, events);
  PlatformConfig pc;
  pc.Backend = DisplayBackendKind::Auto;
  PlatformModule platform {pc};

  GraphicsConfig cfg {};
  cfg.Backend = GraphicsBackend::Vulkan;
  cfg.Debug = false;
  cfg.AppName = "gecko-visible-test";
  GraphicsModule graphics {cfg};

  IModule* modules[] = {&runtime, &platform, &graphics};
  auto engine = Engine::Create(modules);
  REQUIRE(engine.has_value());

  auto* device = GetGraphicsDevice();
  REQUIRE(device != nullptr);

  WindowDesc wd {};
  wd.Title = "Gecko visible feature test";
  wd.Visible = true;
  wd.Size = {640, 360};
  WindowHandle win = GetWindows()->CreateWindow(wd);
  REQUIRE(win.IsValid());

  NativeWindowHandle native = GetWindows()->GetNativeWindowHandle(win);

  Extent2D sz = GetWindows()->GetClientSize(win);
  SwapchainDesc scDesc {};
  scDesc.Width = sz.Width;
  scDesc.Height = sz.Height;
  scDesc.NumBackBuffers = 2;
  Swapchain sc = device->CreateSwapchain(native, scDesc);

  // Pump events for ~750ms so the window is visible long enough to
  // see a flash on screen. The test still runs headless on CI when
  // [.visible] is filtered out.
  using namespace ::std::chrono_literals;
  auto deadline = ::std::chrono::steady_clock::now() + 750ms;
  while (::std::chrono::steady_clock::now() < deadline)
  {
    PumpEvents();
    ::std::this_thread::sleep_for(16ms);
  }

  GetWindows()->DestroyWindow(win);
  // Destroy GPU resources before tearing down the GraphicsModule (which
  // owns the device). Reassigning to a default-constructed Swapchain
  // releases the previous one through its RAII deleter.
  sc = Swapchain {};
  engine.reset();
  ResetAllocator();
}
#endif
