/// @file
/// Feature test for the runtime module / service stack.
///
/// Boots a real `RuntimeModule` with the production
/// `ThreadPoolJobSystem`, `RingProfiler`, `ImmediateLogger`, and
/// `EventBus` implementations, exercises the publish/lookup
/// contract end-to-end, then asserts a clean tear-down.
///
/// Lives in the feature suite because it spins up real worker
/// threads (via `ThreadPoolJobSystem`) and does cross-thread event
/// dispatch -- both nondeterministic enough that a bug here is
/// genuinely an *integration* bug, not a unit-level one.

#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/immediate_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/runtime_module.h"
#include "gecko/runtime/thread_pool_job_system.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <thread>
#include <vector>

using namespace gecko;
using namespace gecko::runtime;

namespace {

constexpr Label TestLabel = MakeLabel("feature.runtime");
constexpr u32 LocalEventCode = 0x42;

}  // namespace

TEST_CASE("RuntimeModule publishes the four foundational services in order",
          "[feature][runtime][module]")
{
  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  ThreadPoolJobSystem jobs;
  RingProfiler profiler {1 << 16};
  ImmediateLogger logger;
  EventBus events;
  RuntimeModule runtime(jobs, profiler, logger, events);

  auto engine = Engine::Create({&runtime});
  REQUIRE(engine.has_value());

  auto& reg = engine->Modules();
  REQUIRE(reg.Service<IJobSystem>() == &jobs);
  REQUIRE(reg.Service<IProfiler>() == &profiler);
  REQUIRE(reg.Service<ILogger>() == &logger);
  REQUIRE(reg.Service<IEventBus>() == &events);

  // Free-function accessors must agree with registry lookups.
  REQUIRE(GetJobSystem() == &jobs);
  REQUIRE(GetProfiler() == &profiler);
  REQUIRE(GetLogger() == &logger);
  REQUIRE(GetEventBus() == &events);

  engine.reset();
  ResetAllocator();
}

TEST_CASE("Job system dispatches work end-to-end through the registry",
          "[feature][runtime][jobs]")
{
  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  ThreadPoolJobSystem jobs;
  jobs.SetWorkerThreadCount(4);
  RingProfiler profiler {1 << 16};
  ImmediateLogger logger;
  EventBus events;
  RuntimeModule runtime(jobs, profiler, logger, events);

  auto engine = Engine::Create({&runtime});
  REQUIRE(engine.has_value());

  ::std::atomic<int> counter {0};
  constexpr int N = 64;
  ::std::vector<JobHandle> handles;
  handles.reserve(N);
  for (int i = 0; i < N; ++i)
  {
    handles.push_back(GetJobSystem()->Submit([&counter]() noexcept {
      counter.fetch_add(1, ::std::memory_order_relaxed);
    }));
  }
  GetJobSystem()->WaitAll(handles.data(), static_cast<u32>(handles.size()));
  REQUIRE(counter.load() == N);

  engine.reset();
  ResetAllocator();
}

TEST_CASE("Event bus delivers events through the runtime stack",
          "[feature][runtime][events]")
{
  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  ThreadPoolJobSystem jobs;
  RingProfiler profiler {1 << 16};
  ImmediateLogger logger;
  EventBus events;
  RuntimeModule runtime(jobs, profiler, logger, events);

  auto engine = Engine::Create({&runtime});
  REQUIRE(engine.has_value());

  auto* bus = GetEventBus();
  REQUIRE(bus != nullptr);
  REQUIRE(bus->RegisterModule(TestLabel.Id));

  const EventCode code = MakeEventCode(TestLabel.Id, LocalEventCode);
  ::std::atomic<int> hits {0};

  auto sub =
      bus->Subscribe(code,
                     [](void* user, const EventMeta&, EventView) {
                       static_cast<::std::atomic<int>*>(user)->fetch_add(1);
                     },
                     &hits, {.delivery = SubscriptionDelivery::Immediate});

  EventEmitter emitter = bus->CreateEmitter(TestLabel.Id, 0);
  for (int i = 0; i < 8; ++i)
    bus->Send(emitter, code, EventView {});
  REQUIRE(hits.load() == 8);

  bus->UnregisterModule(TestLabel.Id);

  engine.reset();
  ResetAllocator();
}

TEST_CASE("Module restart works repeatedly with the runtime stack",
          "[feature][runtime][module]")
{
  for (int iter = 0; iter < 4; ++iter)
  {
    SystemAllocator alloc;
    REQUIRE(SetAllocator(&alloc));

    ThreadPoolJobSystem jobs;
    RingProfiler profiler {1 << 16};
    ImmediateLogger logger;
    EventBus events;
    RuntimeModule runtime(jobs, profiler, logger, events);

    auto engine = Engine::Create({&runtime});
    REQUIRE(engine.has_value());
    REQUIRE(GetJobSystem() == &jobs);

    engine.reset();
    ResetAllocator();
  }

  // After the last cycle, the registry must report the Null fallbacks.
  REQUIRE(GetJobSystem() != nullptr);  // Null fallback, not nullptr.
}
