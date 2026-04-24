#pragma once

#include "gecko/core/services.h"
#include "gecko/runtime/event_bus.h"

#include <catch2/catch_test_macros.hpp>

namespace gecko::test {

/// RAII scope that boots Gecko services with Null implementations.
/// Use in any test that needs a working service layer.
struct TestServiceScope
{
  SystemAllocator Alloc;
  NullJobSystem Jobs;
  NullProfiler Profiler;
  NullLogger Logger;
  NullModuleRegistry Modules;
  runtime::EventBus Events;

  TestServiceScope()
  {
    (void)SetAllocator(&Alloc);
    Jobs.Init();
    Profiler.Init();
    Logger.Init();
    (void)Modules.Init();
    (void)Events.Init();

    Services svc {
        .JobSystem = &Jobs,
        .Profiler = &Profiler,
        .Logger = &Logger,
        .Modules = &Modules,
        .EventBus = &Events,
    };
    (void)InstallServices(svc);
  }

  ~TestServiceScope()
  {
    UninstallServices();
    ResetAllocator();
  }

  TestServiceScope(const TestServiceScope&) = delete;
  TestServiceScope& operator=(const TestServiceScope&) = delete;
};

}  // namespace gecko::test
