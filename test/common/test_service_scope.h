#pragma once

#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/runtime/core_module.h"
#include "gecko/runtime/event_bus.h"

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
  runtime::CoreModule Core;
  ::std::optional<::gecko::Engine> EngineHandle;

  TestServiceScope() : Core(Jobs, Profiler, Logger, Events)
  {
    REQUIRE(SetAllocator(&Alloc));
    EngineHandle = ::gecko::Engine::Create({&Core});
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
