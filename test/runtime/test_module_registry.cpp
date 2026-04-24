#include "gecko/core/services.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/module_registry.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::runtime;

namespace {

struct TestServiceScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  ModuleRegistry modules;
  EventBus eventBus;

  TestServiceScope()
  {
    (void)SetAllocator(&alloc);
    jobs.Init();
    profiler.Init();
    logger.Init();
    eventBus.Init();
    (void)modules.Init();

    Services svc {
        .JobSystem = &jobs,
        .Profiler = &profiler,
        .Logger = &logger,
        .Modules = &modules,
        .EventBus = &eventBus,
    };
    (void)InstallServices(svc);
  }

  ~TestServiceScope()
  {
    UninstallServices();
    ResetAllocator();
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

}  // namespace

TEST_CASE("ModuleRegistry Init/Shutdown", "[runtime][modules]")
{
  TestServiceScope scope;
}

TEST_CASE("ModuleRegistry register and find module", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.module"};
  auto reg = scope.modules.RegisterStatic(mod);
  REQUIRE(reg.Ok());

  IModule* found = scope.modules.GetModule(mod.m_Label);
  REQUIRE(found == &mod);

  scope.modules.ShutdownAllModules();
}

TEST_CASE("ModuleRegistry duplicate registration fails", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.dup"};
  auto reg1 = scope.modules.RegisterStatic(mod);
  REQUIRE(reg1.Ok());

  auto reg2 = scope.modules.RegisterStatic(mod);
  REQUIRE(reg2.Result == ModuleResult::DuplicateModule);

  scope.modules.ShutdownAllModules();
}

TEST_CASE("ModuleRegistry unregister module", "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.unreg"};
  auto reg = scope.modules.RegisterStatic(mod);
  REQUIRE(reg.Ok());
  reg.Handle.Release();

  auto result = scope.modules.Unregister(mod.m_Label);
  REQUIRE(result == ModuleResult::Ok);

  IModule* found = scope.modules.GetModule(mod.m_Label);
  REQUIRE(found == nullptr);
}

TEST_CASE("ModuleRegistry StartupAllModules calls Startup",
          "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod1 {"test.mod1"};
  MockModule mod2 {"test.mod2"};

  auto reg1 = scope.modules.RegisterStatic(mod1);
  auto reg2 = scope.modules.RegisterStatic(mod2);
  REQUIRE(reg1.Ok());
  REQUIRE(reg2.Ok());

  REQUIRE(scope.modules.StartupAllModules());
  REQUIRE(mod1.m_StartupCalled);
  REQUIRE(mod2.m_StartupCalled);

  scope.modules.ShutdownAllModules();
}

TEST_CASE("ModuleRegistry ShutdownAllModules calls Shutdown",
          "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod {"test.shutdown"};
  auto reg = scope.modules.RegisterStatic(mod);
  REQUIRE(reg.Ok());

  REQUIRE(scope.modules.StartupAllModules());
  scope.modules.ShutdownAllModules();
  REQUIRE(mod.m_ShutdownCalled);
}

TEST_CASE("ModuleRegistry GetModule returns null for unknown",
          "[runtime][modules]")
{
  TestServiceScope scope;

  Label unknown = MakeLabel("nonexistent.module");
  REQUIRE(scope.modules.GetModule(unknown) == nullptr);
}

TEST_CASE("ModuleRegistry ForEachModule visits all modules",
          "[runtime][modules]")
{
  TestServiceScope scope;

  MockModule mod1 {"test.each1"};
  MockModule mod2 {"test.each2"};
  auto reg1 = scope.modules.RegisterStatic(mod1);
  auto reg2 = scope.modules.RegisterStatic(mod2);

  int visitCount = 0;
  scope.modules.ForEachModule(
      [](IModule&, bool, void* user) noexcept {
        *static_cast<int*>(user) += 1;
      },
      &visitCount);

  REQUIRE(visitCount == 2);

  scope.modules.ShutdownAllModules();
}
