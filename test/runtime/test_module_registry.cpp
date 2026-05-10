#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/platform/platform_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <optional>

using namespace gecko;
using namespace gecko::runtime;

namespace {

struct TestServiceScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  EventBus eventBus;
  RuntimeModule runtimeMod;
  ::std::optional<::gecko::Engine> engine;

  TestServiceScope() : runtimeMod(jobs, profiler, logger, eventBus)
  {
    REQUIRE(SetAllocator(&alloc));
    engine = ::gecko::Engine::Create({&runtimeMod});
  }

  ~TestServiceScope()
  {
    engine.reset();
    ResetAllocator();
  }

  IModuleRegistry& modules() noexcept
  {
    return engine->Modules();
  }
};

class MockModule : public IModule
{
public:
  Label m_Label;
  bool m_StartupCalled = false;
  bool m_ShutdownCalled = false;
  bool m_StartupResult = true;

  explicit MockModule(const char* name, bool startupResult = true)
      : m_Label(MakeLabel(name)), m_StartupResult(startupResult)
  {}

  Label RootLabel() const noexcept override
  {
    return m_Label;
  }

  bool Startup(IModuleRegistry&) noexcept override
  {
    m_StartupCalled = true;
    return m_StartupResult;
  }

  void Shutdown(IModuleRegistry&) noexcept override
  {
    m_ShutdownCalled = true;
  }
};

// Module that records the live service pointers it observes during
// Startup/Shutdown. Declares Requires() over the four foundational
// services so the topological sort orders the publisher before us.
class ServiceSpyModule : public IModule
{
public:
  Label m_Label;
  IJobSystem* m_StartupJobs = nullptr;
  IProfiler* m_StartupProfiler = nullptr;
  ILogger* m_StartupLogger = nullptr;
  IEventBus* m_StartupEvents = nullptr;
  IJobSystem* m_ShutdownJobs = nullptr;

  explicit ServiceSpyModule(const char* name) : m_Label(MakeLabel(name))
  {}

  Label RootLabel() const noexcept override
  {
    return m_Label;
  }

  ::gecko::Span<const ::gecko::ServiceId> Requires() const noexcept override
  {
    static constexpr ::gecko::ServiceId required[] = {
        ::gecko::ServiceIdOf<IJobSystem>(),
        ::gecko::ServiceIdOf<IProfiler>(),
        ::gecko::ServiceIdOf<ILogger>(),
        ::gecko::ServiceIdOf<IEventBus>(),
    };
    return ::gecko::Span<const ::gecko::ServiceId> {required};
  }

  bool Startup(IModuleRegistry&) noexcept override
  {
    m_StartupJobs = GetJobSystem();
    m_StartupProfiler = GetProfiler();
    m_StartupLogger = GetLogger();
    m_StartupEvents = GetEventBus();
    return true;
  }

  void Shutdown(IModuleRegistry&) noexcept override
  {
    m_ShutdownJobs = GetJobSystem();
  }
};

}  // namespace

TEST_CASE("ModuleRegistry Init/Shutdown", "[runtime][modules]")
{
  TestServiceScope scope;
}

TEST_CASE("ModuleRegistry register and find module", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.module"};
  auto reg = scope.modules().RegisterStatic(mod);
  REQUIRE(reg.Ok());

  IModule* found = scope.modules().GetModule(mod.m_Label);
  REQUIRE(found == &mod);

  scope.modules().ShutdownAllModules();
}

TEST_CASE("ModuleRegistry duplicate registration fails", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.dup"};
  auto reg1 = scope.modules().RegisterStatic(mod);
  REQUIRE(reg1.Ok());

  auto reg2 = scope.modules().RegisterStatic(mod);
  REQUIRE(reg2.Result == ModuleResult::DuplicateModule);

  scope.modules().ShutdownAllModules();
}

TEST_CASE("ModuleRegistry unregister module", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.unreg"};
  auto reg = scope.modules().RegisterStatic(mod);
  REQUIRE(reg.Ok());
  reg.Handle.Release();

  auto result = scope.modules().Unregister(mod.m_Label);
  REQUIRE(result == ModuleResult::Ok);

  IModule* found = scope.modules().GetModule(mod.m_Label);
  REQUIRE(found == nullptr);
}

TEST_CASE("ModuleRegistry StartupAllModules calls Startup", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod1 {"test.mod1"};
  MockModule mod2 {"test.mod2"};

  auto reg1 = scope.modules().RegisterStatic(mod1);
  auto reg2 = scope.modules().RegisterStatic(mod2);
  REQUIRE(reg1.Ok());
  REQUIRE(reg2.Ok());

  REQUIRE(scope.modules().StartupAllModules());
  REQUIRE(mod1.m_StartupCalled);
  REQUIRE(mod2.m_StartupCalled);

  scope.modules().ShutdownAllModules();
}

TEST_CASE("ModuleRegistry ShutdownAllModules calls Shutdown", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.shutdown"};
  auto reg = scope.modules().RegisterStatic(mod);
  REQUIRE(reg.Ok());

  REQUIRE(scope.modules().StartupAllModules());
  scope.modules().ShutdownAllModules();
  REQUIRE(mod.m_ShutdownCalled);
}

TEST_CASE("ModuleRegistry GetModule returns null for unknown", "[runtime][modules]")
{
  TestServiceScope scope;

  Label unknown = MakeLabel("nonexistent.module");
  REQUIRE(scope.modules().GetModule(unknown) == nullptr);
}

TEST_CASE("ModuleRegistry ForEachModule visits all modules", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod1 {"test.each1"};
  MockModule mod2 {"test.each2"};
  auto reg1 = scope.modules().RegisterStatic(mod1);
  auto reg2 = scope.modules().RegisterStatic(mod2);

  int visitCount = 0;
  scope.modules().ForEachModule([](IModule&, bool, void* user) noexcept { *static_cast<int*>(user) += 1; },
                                &visitCount);

  // Engine starts the test scope with a RuntimeModule already
  // registered; the two MockModules registered above bring the total to three.
  REQUIRE(visitCount == 3);

  scope.modules().ShutdownAllModules();
}

TEST_CASE("Topological sort starts services-publisher before a Requires()-declaring "
          "module so its Startup observes live, non-null service implementations",
          "[runtime][modules][topo]")
{
  // Build the engine ourselves so we can register the spy module up
  // front and observe the topological start order. The spy is
  // intentionally registered *before* RuntimeModule to prove that
  // ordering is driven by Requires()/Publishes(), not registration
  // order.
  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  EventBus events;
  RuntimeModule services(jobs, profiler, logger, events);
  ServiceSpyModule spy {"test.spy"};

  auto engine = ::gecko::Engine::Create({&spy, &services});
  REQUIRE(engine.has_value());

  // The four service pointers observed during spy.Startup must point
  // to the user-supplied implementations (jobs/profiler/logger/events),
  // not the global Null fallbacks. If the topo sort ran services first,
  // GetX() routes through the live registry and returns those.
  REQUIRE(spy.m_StartupJobs == &jobs);
  REQUIRE(spy.m_StartupProfiler == &profiler);
  REQUIRE(spy.m_StartupLogger == &logger);
  REQUIRE(spy.m_StartupEvents == &events);

  engine.reset();

  // Shutdown order is the reverse of startup, so the spy must have
  // shut down BEFORE services were unpublished -> still live.
  REQUIRE(spy.m_ShutdownJobs == &jobs);

  ResetAllocator();
}

TEST_CASE("Topological sort orders Platform and Graphics after Runtime", "[runtime][modules][topo]")
{
  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  EventBus events;
  RuntimeModule runtime(jobs, profiler, logger, events);
  ::gecko::platform::PlatformModule platform {};
  ::gecko::graphics::GraphicsModule graphics {};

  // Register graphics first to prove ordering is driven by the
  // dependency graph, not registration order.
  auto engine = ::gecko::Engine::Create({&graphics, &platform, &runtime});
  REQUIRE(engine.has_value());

  // All published services must be visible after Startup.
  REQUIRE(engine->Modules().Service<IJobSystem>() == &jobs);
  REQUIRE(engine->Modules().Service<::gecko::graphics::GraphicsDevice>() != nullptr);
  REQUIRE(::gecko::platform::GetWindows() != nullptr);
  REQUIRE(::gecko::graphics::GetGraphicsDevice() != nullptr);

  engine.reset();
  REQUIRE(::gecko::graphics::GetGraphicsDevice() == nullptr);
  REQUIRE(::gecko::platform::GetWindows() == nullptr);

  ResetAllocator();
}
