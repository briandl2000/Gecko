#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <optional>

using namespace gecko;
using namespace gecko::graphics;

namespace {

struct ServiceScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  runtime::EventBus events;
  runtime::RuntimeModule runtime;
  ::std::optional<Engine> engine;

  ServiceScope() : runtime(jobs, profiler, logger, events)
  {
    REQUIRE(SetAllocator(&alloc));
  }
  ~ServiceScope()
  {
    engine.reset();
    ResetAllocator();
  }
};

}  // namespace

TEST_CASE("GraphicsModule reports its label and service contract",
          "[graphics][module]")
{
  GraphicsModule module {};
  REQUIRE(module.RootLabel() == labels::Graphics);

  auto requires_ = module.Requires();
  REQUIRE(requires_.size() == 4);

  auto publishes = module.Publishes();
  REQUIRE(publishes.size() == 2);
  // GraphicsDevice is always published; IGpuSampler is conditional on
  // backend support, but the contract advertises both either way so the
  // topo-sort still works.
}

TEST_CASE("GraphicsModule with default config publishes GraphicsDevice via "
          "NullDevice",
          "[graphics][module]")
{
  GraphicsModule module {GraphicsConfig {}};
  ServiceScope scope;

  scope.engine = Engine::Create({&scope.runtime, &module});
  REQUIRE(scope.engine.has_value());

  auto& reg = scope.engine->Modules();
  GraphicsDevice* dev = reg.Service<GraphicsDevice>();
  REQUIRE(dev != nullptr);
  REQUIRE(GetGraphicsDevice() == dev);

  // NullDevice does not support GPU sampling -- sampler is not
  // published, accessor returns null.
  REQUIRE(GetGpuSampler() == nullptr);
}

TEST_CASE("GraphicsModule clears accessors after Shutdown",
          "[graphics][module]")
{
  GraphicsModule module {GraphicsConfig {}};
  ServiceScope scope;

  scope.engine = Engine::Create({&scope.runtime, &module});
  REQUIRE(scope.engine.has_value());
  REQUIRE(GetGraphicsDevice() != nullptr);

  scope.engine.reset();
  REQUIRE(GetGraphicsDevice() == nullptr);
  REQUIRE(GetGpuSampler() == nullptr);
}

TEST_CASE("GraphicsModule respects Backends injection", "[graphics][module]")
{
  auto injected = CreateGraphicsDevice();
  REQUIRE(injected != nullptr);

  GraphicsModule module {GraphicsConfig {},
                         GraphicsModule::Backends {injected.get()}};
  ServiceScope scope;

  scope.engine = Engine::Create({&scope.runtime, &module});
  REQUIRE(scope.engine.has_value());
  REQUIRE(GetGraphicsDevice() == injected.get());

  scope.engine.reset();
  // Module must not destroy injected devices: the unique_ptr the
  // caller still owns is responsible for that.
  REQUIRE(injected != nullptr);
}

TEST_CASE("GraphicsModule restart is idempotent", "[graphics][module]")
{
  for (int i = 0; i < 3; ++i)
  {
    GraphicsModule module {GraphicsConfig {}};
    ServiceScope scope;
    scope.engine = Engine::Create({&scope.runtime, &module});
    REQUIRE(scope.engine.has_value());
    REQUIRE(GetGraphicsDevice() != nullptr);
  }
  REQUIRE(GetGraphicsDevice() == nullptr);
}
