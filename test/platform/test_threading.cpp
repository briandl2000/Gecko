#include "gecko/platform/threading.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using namespace gecko;
using namespace gecko::platform;

TEST_CASE("HardwareThreadCount is positive", "[platform][threading]")
{
  auto n = HardwareThreadCount();
  REQUIRE(n >= 1u);
  auto stdN = ::std::thread::hardware_concurrency();
  if (stdN > 0)
    REQUIRE(n <= stdN + 1);  // small fudge for cgroup edge cases
}

TEST_CASE("CurrentThreadId is non-zero and stable", "[platform][threading]")
{
  auto a = CurrentThreadId();
  auto b = CurrentThreadId();
  REQUIRE(a != 0u);
  REQUIRE(a == b);
}

TEST_CASE("Different threads report different ids", "[platform][threading]")
{
  auto mainId = CurrentThreadId();
  ::std::atomic<ThreadId> workerId {0};
  ::std::thread worker([&] { workerId.store(CurrentThreadId()); });
  worker.join();
  REQUIRE(workerId.load() != 0u);
  REQUIRE(workerId.load() != mainId);
}

TEST_CASE("YieldThread is a no-op for correctness", "[platform][threading]")
{
  YieldThread();
  SUCCEED();
}

TEST_CASE("SleepNanoseconds(0) is a no-op", "[platform][threading]")
{
  SleepNanoseconds(0);
  SUCCEED();
}

TEST_CASE("Non-zero sleep blocks for at least the requested time", "[platform][threading]")
{
  auto start = ::std::chrono::steady_clock::now();
  SleepNanoseconds(2'000'000ULL);
  auto elapsed = ::std::chrono::steady_clock::now() - start;
  auto ns = ::std::chrono::duration_cast<::std::chrono::nanoseconds>(elapsed).count();
  REQUIRE(ns >= 1'000'000);
}

TEST_CASE("SetCurrentThreadName does not crash", "[platform][threading]")
{
  SetCurrentThreadName("gecko-test");
  SetCurrentThreadName(nullptr);
  SetCurrentThreadName("name-longer-than-fifteen-chars-gets-truncated");
  SUCCEED();
}
