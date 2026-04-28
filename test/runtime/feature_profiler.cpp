// Profiler feature tests.
//
// Unlike the unit tests in test_ring_profiler.cpp, these tests stand up a
// real ThreadPoolJobSystem so the consumer-job path runs on actual worker
// threads, AsyncTraceProfilerSink writes to a real file under contention,
// and the aggregator's open-scope TLS stack is exercised across many
// independent threads. They run headless (no display required) but need
// a few seconds of wall time, hence "feature" rather than "unit".

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gecko/core/engine.h>
#include <gecko/core/services.h>
#include <gecko/core/services/jobs.h>
#include <gecko/core/services/profiler.h>
#include <gecko/core/utility/hash.h>
#include <gecko/runtime/async_trace_profiler_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace ::gecko;
using namespace ::gecko::runtime;

namespace {

ProfEvent MakeBegin(u32 hash, const char* name, u64 ts, u32 tid = 0) noexcept
{
  ProfEvent e {};
  e.TimestampNs = ts;
  e.Name = name;
  e.NameHash = hash;
  e.ThreadId = tid;
  e.Kind = ProfEventKind::ZoneBegin;
  e.Level = ProfLevel::Always;
  return e;
}

ProfEvent MakeEnd(u32 hash, const char* name, u64 ts, u32 tid = 0) noexcept
{
  ProfEvent e {};
  e.TimestampNs = ts;
  e.Name = name;
  e.NameHash = hash;
  e.ThreadId = tid;
  e.Kind = ProfEventKind::ZoneEnd;
  e.Level = ProfLevel::Always;
  return e;
}

u64 NowNs() noexcept
{
  return static_cast<u64>(
      ::std::chrono::duration_cast<::std::chrono::nanoseconds>(
          ::std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

::std::string SlurpFile(const ::std::string& path)
{
  ::std::ifstream f(path, ::std::ios::binary);
  ::std::ostringstream oss;
  oss << f.rdbuf();
  return oss.str();
}

::std::string TempTracePath(const char* tag)
{
  auto p = ::std::filesystem::temp_directory_path() /
           (::std::string("gecko_feature_trace_") + tag + ".json");
  ::std::error_code ec;
  ::std::filesystem::remove(p, ec);
  return p.string();
}

}  // namespace

TEST_CASE("Profiler feature: high-contention multi-thread workload",
          "[runtime][profiler][feature]")
{
  // Many threads pound the same set of NameHashes through a real ring of
  // moderate size. We expect every Begin/End pair to land in the
  // aggregator with no double-counts and no lost samples - the TLS
  // open-scope stack is the only thing that makes this possible.
  RingProfiler prof(1 << 14);
  REQUIRE(prof.Init());
  prof.SetStatsResetIntervalMs(0);

  const u32 hashA = ::gecko::FNV1a("FeatA");
  const u32 hashB = ::gecko::FNV1a("FeatB");

  constexpr u32 NumThreads = 16;
  constexpr u32 IterPerThread = 200;

  ::std::atomic<bool> go {false};
  ::std::vector<::std::thread> ts;
  ts.reserve(NumThreads);
  for (u32 t = 0; t < NumThreads; ++t)
  {
    ts.emplace_back([&, t]() {
      while (!go.load(::std::memory_order_acquire))
      {}
      for (u32 i = 0; i < IterPerThread; ++i)
      {
        // Nested scopes: outer A, inner B. Tests both cross-thread A
        // pairing and proper LIFO matching of the inner B scope on each
        // thread's TLS stack.
        u64 t0 = NowNs();
        prof.Emit(MakeBegin(hashA, "FeatA", t0, t + 1));
        u64 t1 = NowNs();
        prof.Emit(MakeBegin(hashB, "FeatB", t1, t + 1));
        // simulate a tiny bit of work
        ::std::this_thread::yield();
        u64 t2 = NowNs();
        prof.Emit(MakeEnd(hashB, "FeatB", t2, t + 1));
        u64 t3 = NowNs();
        prof.Emit(MakeEnd(hashA, "FeatA", t3, t + 1));
      }
    });
  }
  go.store(true, ::std::memory_order_release);
  for (auto& th : ts)
    th.join();

  ScopeStats sa = prof.GetStats(hashA);
  ScopeStats sb = prof.GetStats(hashB);
  REQUIRE(sa.Count == NumThreads * IterPerThread);
  REQUIRE(sb.Count == NumThreads * IterPerThread);
  // Outer scope must always be at least as long as inner.
  REQUIRE(sa.MaxNs >= sb.MaxNs);

  prof.Shutdown();
}

TEST_CASE("Profiler feature: real ThreadPoolJobSystem drives the consumer",
          "[runtime][profiler][feature]")
{
  // With a real job system installed via Engine, RingProfiler's
  // TryScheduleConsumerJob hands its consumer job off to a worker thread
  // instead of running it inline. Verify that events still flow without
  // explicit Flush(), no events are lost across many emit waves, and
  // shutdown drains the tail.
  SystemAllocator alloc;
  REQUIRE(SetAllocator(&alloc));

  ThreadPoolJobSystem jobs;
  NullProfiler nullProf;
  NullLogger logger;
  EventBus eventBus;
  CoreServicesModule coreMod(jobs, nullProf, logger, eventBus);

  {
    auto engine = ::gecko::Engine::Create({&coreMod});

    RingProfiler prof(1 << 13);
    REQUIRE(prof.Init());

    struct CountingSink : IProfilerSink
    {
      ::std::atomic<u32> Count {0};
      void Write(const ProfEvent&) noexcept override
      {
        Count.fetch_add(1, ::std::memory_order_relaxed);
      }
      void WriteBatch(::gecko::Span<const ProfEvent> evs) noexcept override
      {
        Count.fetch_add(static_cast<u32>(evs.size()),
                        ::std::memory_order_relaxed);
      }
      void Flush() noexcept override
      {}
    };
    CountingSink sink;
    prof.AddSink(&sink);

    const u32 hash = ::gecko::FNV1a("JobsBacked");
    constexpr u32 NumWaves = 5;
    constexpr u32 EmitsPerWave = 200;

    for (u32 w = 0; w < NumWaves; ++w)
    {
      for (u32 i = 0; i < EmitsPerWave; ++i)
      {
        u64 base = NowNs();
        prof.Emit(MakeBegin(hash, "JobsBacked", base));
        prof.Emit(MakeEnd(hash, "JobsBacked", base + 50));
      }
      // Yield so the worker thread can drain the ring asynchronously.
      ::std::this_thread::sleep_for(::std::chrono::milliseconds(2));
    }

    prof.RemoveSink(&sink);
    prof.Shutdown();

    // Total events emitted: NumWaves * EmitsPerWave * 2 (begin + end).
    // After Shutdown the ring is fully drained, so the sink must have
    // received exactly that count (no drops, no duplicates).
    const u32 expected = NumWaves * EmitsPerWave * 2;
    REQUIRE(sink.Count.load(::std::memory_order_relaxed) == expected);
  }
  // engine destructor runs CoreServicesModule shutdown which stops jobs

  ResetAllocator();
}

TEST_CASE("Profiler feature: AsyncTraceProfilerSink under multi-thread load",
          "[runtime][profiler][feature][trace]")
{
  // End-to-end: write a chrome trace from many threads via the real sink,
  // then re-open and validate the produced JSON file is well-formed and
  // contains the expected scope names.
  const auto path = TempTracePath("multithread");

  {
    RingProfiler prof(1 << 13);
    REQUIRE(prof.Init());

    AsyncTraceProfilerSink sink(path.c_str());
    REQUIRE(sink.IsOpen());
    prof.AddSink(&sink);

    constexpr u32 NumThreads = 8;
    constexpr u32 IterPerThread = 100;
    ::std::atomic<bool> go {false};
    ::std::vector<::std::thread> ts;
    ts.reserve(NumThreads);
    for (u32 t = 0; t < NumThreads; ++t)
    {
      ts.emplace_back([&, t]() {
        const u32 h = ::gecko::FNV1a("TraceZone");
        while (!go.load(::std::memory_order_acquire))
        {}
        for (u32 i = 0; i < IterPerThread; ++i)
        {
          u64 b = NowNs();
          prof.Emit(MakeBegin(h, "TraceZone", b, t + 1));
          u64 e = NowNs();
          prof.Emit(MakeEnd(h, "TraceZone", e, t + 1));
        }
      });
    }
    go.store(true, ::std::memory_order_release);
    for (auto& th : ts)
      th.join();

    prof.RemoveSink(&sink);
    prof.Shutdown();
    // sink dtor here finishes the JSON document
  }

  const auto contents = SlurpFile(path);
  REQUIRE(!contents.empty());
  REQUIRE(contents.front() == '{');
  REQUIRE(contents.back() == '}');
  REQUIRE(contents.find("\"traceEvents\":[") != ::std::string::npos);
  REQUIRE(contents.find("TraceZone") != ::std::string::npos);
  REQUIRE(contents.find(",,") == ::std::string::npos);
  REQUIRE(contents.find("][") == ::std::string::npos);  // no broken arrays

  ::std::filesystem::remove(path);
}
