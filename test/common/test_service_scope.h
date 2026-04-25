#pragma once

#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <optional>

namespace gecko::test {

/// RAII scope that boots Gecko services with Null implementations.
/// Use in any test that needs a working service layer.
struct TestServiceScope
{
  SystemAllocator Alloc;
  NullJobSystem Jobs;
  NullProfiler Profiler;
  NullLogger Logger;
  runtime::EventBus Events;
  runtime::CoreServicesModule Runtime;
  ::std::optional<::gecko::Engine> EngineHandle;

  TestServiceScope() : Runtime(Jobs, Profiler, Logger, Events)
  {
    REQUIRE(SetAllocator(&Alloc));
    EngineHandle = ::gecko::Engine::Create({&Runtime});
    REQUIRE(EngineHandle.has_value());
  }

  ~TestServiceScope()
  {
    EngineHandle.reset();
    ResetAllocator();
  }

  TestServiceScope(const TestServiceScope&) = delete;
  TestServiceScope& operator=(const TestServiceScope&) = delete;
};

}  // namespace gecko::test
