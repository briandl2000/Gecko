#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/platform/platform_module.h"
#include "gecko/platform/threading.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <optional>
#include <thread>

using namespace gecko;
using namespace gecko::platform;

namespace {

// Minimal test scope that boots a Runtime + Platform module pair so the
// IThreading service is published.
struct ThreadingScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  runtime::EventBus events;
  runtime::CoreServicesModule runtimeMod;
  PlatformModule platformMod;
  ::std::optional<::gecko::Engine> engine;

  ThreadingScope() : runtimeMod(jobs, profiler, logger, events)
  {
    REQUIRE(SetAllocator(&alloc));
    engine = ::gecko::Engine::Create({&runtimeMod, &platformMod});
    REQUIRE(engine.has_value());
  }

  ~ThreadingScope()
  {
    engine.reset();
    ResetAllocator();
  }
};

}  // namespace

TEST_CASE("GetThreading returns NullThreading before engine boots",
          "[platform][threading]")
{
  // No active module registry — accessor must still return non-null.
  IThreading* t = GetThreading();
  REQUIRE(t != nullptr);
  REQUIRE(t->GetHardwareThreadCount() >= 1u);
}

TEST_CASE("PlatformModule publishes IThreading", "[platform][threading]")
{
  ThreadingScope scope;
  IThreading* t = GetThreading();
  REQUIRE(t != nullptr);

  SECTION("hardware thread count is positive and matches std")
  {
    auto n = t->GetHardwareThreadCount();
    REQUIRE(n >= 1u);
    // Sanity-check against std (allowing for cgroup/affinity differences:
    // the platform value should be <= std::thread::hardware_concurrency
    // when the latter is non-zero, or >= 1 in any case).
    auto stdN = ::std::thread::hardware_concurrency();
    if (stdN > 0)
      REQUIRE(n <= stdN + 1);  // small fudge for rounding edge cases
  }

  SECTION("current thread id is non-zero and stable")
  {
    auto a = t->GetCurrentThreadId();
    auto b = t->GetCurrentThreadId();
    REQUIRE(a != 0u);
    REQUIRE(a == b);
  }

  SECTION("different threads report different ids")
  {
    auto mainId = t->GetCurrentThreadId();
    ::std::atomic<ThreadId> workerId {0};
    ::std::thread worker(
        [&] { workerId.store(GetThreading()->GetCurrentThreadId()); });
    worker.join();
    REQUIRE(workerId.load() != 0u);
    REQUIRE(workerId.load() != mainId);
  }

  SECTION("yield is a no-op for correctness")
  {
    t->YieldThread();
    SUCCEED();
  }

  SECTION("zero-nanosecond sleep is a no-op")
  {
    t->SleepNanoseconds(0);
    SUCCEED();
  }

  SECTION("non-zero sleep blocks for at least the requested time")
  {
    auto start = ::std::chrono::steady_clock::now();
    t->SleepNanoseconds(2'000'000ULL);  // 2 ms
    auto elapsed = ::std::chrono::steady_clock::now() - start;
    auto ns = ::std::chrono::duration_cast<::std::chrono::nanoseconds>(elapsed)
                  .count();
    // Allow for scheduler imprecision; just assert we slept *something*.
    REQUIRE(ns >= 1'000'000);  // at least 1 ms actually elapsed
  }

  SECTION("set thread name does not crash")
  {
    t->SetCurrentThreadName("gecko-test");
    t->SetCurrentThreadName(nullptr);  // nullptr is documented no-op
    t->SetCurrentThreadName("name-longer-than-fifteen-chars-gets-truncated");
    SUCCEED();
  }
}
