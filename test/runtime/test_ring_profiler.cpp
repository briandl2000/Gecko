#include "gecko/core/services/profiler.h"
#include "gecko/core/utility/hash.h"
#include "gecko/runtime/ring_profiler.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <vector>

using namespace gecko;
using namespace gecko::runtime;

namespace {

ProfEvent MakeZoneBegin(u32 hash, u64 t, ProfLevel lvl = ProfLevel::Normal) noexcept
{
  ProfEvent e {};
  e.TimestampNs = t;
  e.Name = "Z";
  e.ThreadId = 1;
  e.NameHash = hash;
  e.Kind = ProfEventKind::ZoneBegin;
  e.Level = lvl;
  return e;
}

ProfEvent MakeZoneEnd(u32 hash, u64 t, ProfLevel lvl = ProfLevel::Normal) noexcept
{
  ProfEvent e {};
  e.TimestampNs = t;
  e.Name = "Z";
  e.ThreadId = 1;
  e.NameHash = hash;
  e.Kind = ProfEventKind::ZoneEnd;
  e.Level = lvl;
  return e;
}

ProfEvent MakeFrame(u64 t) noexcept
{
  ProfEvent e {};
  e.TimestampNs = t;
  e.Name = "Frame";
  e.ThreadId = 1;
  e.NameHash = 0;
  e.Kind = ProfEventKind::FrameMark;
  e.Level = ProfLevel::Always;
  return e;
}

}  // namespace

TEST_CASE("RingProfiler basic init/shutdown", "[runtime][profiler]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  REQUIRE(prof.IsLevelEnabled(ProfLevel::Always));
  REQUIRE(prof.IsLevelEnabled(ProfLevel::Normal));
  // Default min level is Detailed in RingProfiler.
  REQUIRE(prof.IsLevelEnabled(ProfLevel::Detailed));

  prof.Shutdown();
}

namespace {
struct RecordingSink : ::gecko::IProfilerSink
{
  ::std::vector<ProfEvent> Events;
  void Write(const ProfEvent& ev) noexcept override
  {
    Events.push_back(ev);
  }
  void WriteBatch(::gecko::Span<const ProfEvent> events) noexcept override
  {
    Events.insert(Events.end(), events.begin(), events.end());
  }
  void Flush() noexcept override
  {}
};
}  // namespace

TEST_CASE("RingProfiler delivers events to sinks in FIFO order", "[runtime][profiler]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  RecordingSink sink;
  prof.AddSink(&sink);

  for (u64 i = 0; i < 10; ++i)
    prof.Emit(MakeZoneBegin(0xAAAA, i + 1));

  // Flush drains anything still queued (rate-limiter may have parked some
  // events between the auto-drain on the first Emit and the next one).
  prof.Flush();

  REQUIRE(sink.Events.size() == 10);
  for (u64 i = 0; i < 10; ++i)
  {
    REQUIRE(sink.Events[i].TimestampNs == i + 1);
    REQUIRE(sink.Events[i].NameHash == 0xAAAAu);
  }

  prof.RemoveSink(&sink);
  prof.Shutdown();
}

TEST_CASE("RingProfiler overflow drops events", "[runtime][profiler]")
{
  RingProfiler prof(8);  // tiny ring
  REQUIRE(prof.Init());
  // Disable auto-draining so the ring really fills. Otherwise on slower
  // hosts (e.g. aarch64) each Emit takes long enough that the 10us
  // schedule gate fires every iteration, draining inline before the next
  // emit can land - and DroppedEvents stays at zero.
  prof.SetAutoScheduleEnabled(false);

  // Without consuming, push more than capacity.
  for (u64 i = 0; i < 64; ++i)
    prof.Emit(MakeZoneBegin(0x1, i + 1));

  ProfilerDiagnostics diag = prof.GetDiagnostics();
  REQUIRE(diag.DroppedEvents > 0);

  prof.Shutdown();
}

TEST_CASE("RingProfiler aggregator min/max/last/count", "[runtime][profiler][aggregator]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  const u32 hash = ::gecko::FNV1a("Foo");

  // Three Always-level zones: durations 100, 50, 200.
  prof.Emit(MakeZoneBegin(hash, 1000, ProfLevel::Always));
  prof.Emit(MakeZoneEnd(hash, 1100, ProfLevel::Always));

  prof.Emit(MakeZoneBegin(hash, 2000, ProfLevel::Always));
  prof.Emit(MakeZoneEnd(hash, 2050, ProfLevel::Always));

  prof.Emit(MakeZoneBegin(hash, 3000, ProfLevel::Always));
  prof.Emit(MakeZoneEnd(hash, 3200, ProfLevel::Always));

  ScopeStats s = prof.GetStats(hash);
  REQUIRE(s.Count == 3u);
  REQUIRE(s.MinNs == 50u);
  REQUIRE(s.MaxNs == 200u);
  REQUIRE(s.LastNs == 200u);

  prof.Shutdown();
}

TEST_CASE("RingProfiler aggregator captures every level", "[runtime][profiler][aggregator]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());
  prof.SetStatsResetIntervalMs(0);  // disable auto-reset

  const u32 hashN = ::gecko::FNV1a("LevelNormal");
  const u32 hashD = ::gecko::FNV1a("LevelDetailed");

  prof.Emit(MakeZoneBegin(hashN, 100, ProfLevel::Normal));
  prof.Emit(MakeZoneEnd(hashN, 200, ProfLevel::Normal));
  prof.Emit(MakeZoneBegin(hashD, 300, ProfLevel::Detailed));
  prof.Emit(MakeZoneEnd(hashD, 350, ProfLevel::Detailed));

  REQUIRE(prof.GetStats(hashN).Count == 1u);
  REQUIRE(prof.GetStats(hashD).Count == 1u);

  prof.Shutdown();
}

TEST_CASE("RingProfiler FrameMark no longer auto-resets aggregator", "[runtime][profiler][aggregator]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());
  prof.SetStatsResetIntervalMs(0);

  const u32 hash = ::gecko::FNV1a("Bar");

  prof.Emit(MakeZoneBegin(hash, 1000, ProfLevel::Always));
  prof.Emit(MakeZoneEnd(hash, 1100, ProfLevel::Always));
  REQUIRE(prof.GetStats(hash).Count == 1u);

  prof.Emit(MakeFrame(2000));
  // FrameMark is now a passive marker — stats survive.
  REQUIRE(prof.GetStats(hash).Count == 1u);

  // Manual reset clears them.
  prof.ResetStats();
  REQUIRE(prof.GetStats(hash).Count == 0u);

  prof.Shutdown();
}

TEST_CASE("RingProfiler categories: register, enable, disable", "[runtime][profiler][categories]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  // Default category 0 is enabled.
  REQUIRE(prof.IsCategoryEnabled(0));

  u8 cat = prof.RegisterCategory("net");
  REQUIRE(cat != 0);
  REQUIRE(cat != ProfInvalidCategory);
  REQUIRE(prof.IsCategoryEnabled(cat));

  // Same name -> same id.
  REQUIRE(prof.RegisterCategory("net") == cat);

  prof.SetCategoryEnabled(cat, false);
  REQUIRE_FALSE(prof.IsCategoryEnabled(cat));

  prof.SetCategoryEnabled(cat, true);
  REQUIRE(prof.IsCategoryEnabled(cat));

  prof.Shutdown();
}

TEST_CASE("RingProfiler diagnostics counters non-decreasing", "[runtime][profiler]")
{
  RingProfiler prof(8);
  REQUIRE(prof.Init());

  ProfilerDiagnostics d0 = prof.GetDiagnostics();
  REQUIRE(d0.DroppedEvents == 0u);
  REQUIRE(d0.ReentrantDrops == 0u);
  REQUIRE(d0.AggregatorOverflow == 0u);

  for (u64 i = 0; i < 64; ++i)
    prof.Emit(MakeZoneBegin(0x1, i + 1));

  ProfilerDiagnostics d1 = prof.GetDiagnostics();
  REQUIRE(d1.DroppedEvents >= d0.DroppedEvents);

  prof.Shutdown();
}

namespace {

struct CountingSink : ::gecko::IProfilerSink
{
  ::std::atomic<u32> Received {0};
  void Write(const ProfEvent&) noexcept override
  {
    Received.fetch_add(1, ::std::memory_order_relaxed);
  }
  void WriteBatch(::gecko::Span<const ProfEvent> events) noexcept override
  {
    Received.fetch_add(static_cast<u32>(events.size()), ::std::memory_order_relaxed);
  }
  void Flush() noexcept override
  {}
};

}  // namespace

TEST_CASE("RingProfiler delivers events to sinks without explicit Flush", "[runtime][profiler][sink]")
{
  // Regression guard: if the consumer-job scheduling path from Emit() ever
  // breaks (e.g. reentrancy guard mis-ordering), this test catches it.
  // Without a real job system installed, GetJobSystem() returns nullptr in
  // RingProfiler::TryScheduleConsumerJob() which then runs ProcessProfEvents
  // inline on the emitting thread - so by the time Emit() returns we expect
  // the event to already be in the sink, with no Flush needed.
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  CountingSink sink;
  prof.AddSink(&sink);

  constexpr u32 NumEvents = 10;
  for (u32 i = 0; i < NumEvents; ++i)
    prof.Emit(MakeZoneBegin(0x1, i + 1));

  // No prof.Flush() here on purpose. We want to see what arrives via the
  // automatic consumer-job path alone.
  u32 receivedBeforeShutdown = sink.Received.load(::std::memory_order_relaxed);

  // Shutdown will Flush internally, draining everything still in the ring.
  prof.RemoveSink(&sink);
  prof.Shutdown();

  // What we really care about: did *any* events make it to the sink before
  // the explicit Flush in RemoveSink/Shutdown? If 0, the consumer path is
  // dead and events only ever flow at shutdown.
  REQUIRE(receivedBeforeShutdown > 0u);
}

TEST_CASE("RingProfiler RegisterCategory rejects null name", "[runtime][profiler][categories]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());
  REQUIRE(prof.RegisterCategory(nullptr) == ProfInvalidCategory);
  prof.Shutdown();
}

TEST_CASE("RingProfiler RegisterCategory copies the name buffer", "[runtime][profiler][categories]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  // Pass a non-static buffer that we then mutate. If the implementation
  // stored the raw pointer, GetCategoryName would either crash or return
  // garbage. With owned-string storage it must still match the original.
  ::std::string scratch = "transient_category";
  u8 cat = prof.RegisterCategory(scratch.c_str());
  REQUIRE(cat != 0);
  REQUIRE(cat != ProfInvalidCategory);

  scratch = "MUTATED_AFTER_REGISTRATION";  // clobber the source buffer
  REQUIRE(::std::string(prof.GetCategoryName(cat)) == "transient_category");

  // Re-registration with the same logical name still returns the same id,
  // even though the new buffer is a different pointer.
  ::std::string again = "transient_category";
  REQUIRE(prof.RegisterCategory(again.c_str()) == cat);

  prof.Shutdown();
}

TEST_CASE("RingProfiler aggregator pairs across threads correctly", "[runtime][profiler][aggregator][threads]")
{
  // Regression guard for the OpenBeginNs cross-thread bug. Before the
  // TLS-stack fix, the same NameHash on multiple threads shared a single
  // OpenBeginNs slot, so each thread's ZoneBegin clobbered the previous
  // thread's pending start time and produced wildly incorrect durations
  // (or silently dropped Count increments).
  RingProfiler prof(1024);
  REQUIRE(prof.Init());
  prof.SetStatsResetIntervalMs(0);

  const u32 hash = ::gecko::FNV1a("CrossThread");
  constexpr u32 NumThreads = 8;
  constexpr u32 IterPerThread = 50;

  ::std::atomic<bool> go {false};
  ::std::vector<::std::thread> ts;
  for (u32 t = 0; t < NumThreads; ++t)
  {
    ts.emplace_back([&, t]() {
      while (!go.load(::std::memory_order_acquire))
      {
        // spin briefly until release
      }
      // Each thread emits IterPerThread complete Begin/End pairs at
      // monotonically increasing timestamps within the thread.
      u64 ts0 = 1'000'000ULL * (t + 1);
      for (u32 i = 0; i < IterPerThread; ++i)
      {
        u64 begin = ts0 + i * 1000;
        u64 end = begin + 100 + (i % 5);  // small varying duration
        ProfEvent eb = MakeZoneBegin(hash, begin, ProfLevel::Always);
        ProfEvent ee = MakeZoneEnd(hash, end, ProfLevel::Always);
        eb.ThreadId = t + 1;
        ee.ThreadId = t + 1;
        prof.Emit(eb);
        prof.Emit(ee);
      }
    });
  }
  go.store(true, ::std::memory_order_release);
  for (auto& th : ts)
    th.join();

  ScopeStats s = prof.GetStats(hash);
  REQUIRE(s.Count == NumThreads * IterPerThread);
  REQUIRE(s.MinNs >= 100u);
  REQUIRE(s.MaxNs <= 200u);  // 100 + (i%5) is at most 104

  prof.Shutdown();
}

TEST_CASE("RingProfiler aggregator handles nested same-name scopes", "[runtime][profiler][aggregator]")
{
  // Nested ZoneBegin with the same hash on one thread used to corrupt
  // OpenBeginNs (inner Begin overwrote the outer's start). With the TLS
  // open-scope stack, each Begin/End matches LIFO and both durations are
  // recorded.
  RingProfiler prof(64);
  REQUIRE(prof.Init());
  prof.SetStatsResetIntervalMs(0);

  const u32 hash = ::gecko::FNV1a("Recursive");

  // Outer scope spans [100, 1000] -> dur 900
  // Inner scope spans [200,  300] -> dur 100
  prof.Emit(MakeZoneBegin(hash, 100, ProfLevel::Always));
  prof.Emit(MakeZoneBegin(hash, 200, ProfLevel::Always));
  prof.Emit(MakeZoneEnd(hash, 300, ProfLevel::Always));
  prof.Emit(MakeZoneEnd(hash, 1000, ProfLevel::Always));

  ScopeStats s = prof.GetStats(hash);
  REQUIRE(s.Count == 2u);
  REQUIRE(s.MinNs == 100u);
  REQUIRE(s.MaxNs == 900u);

  prof.Shutdown();
}

TEST_CASE("RingProfiler aggregator ignores orphan ZoneEnd", "[runtime][profiler][aggregator]")
{
  // A ZoneEnd with no matching open Begin must not bump Count or pollute
  // Min/Max. (Could happen if a ZoneBegin was filtered by category gating
  // before it reached UpdateAggregator while the End slipped through, or
  // if user code emits raw events.)
  RingProfiler prof(64);
  REQUIRE(prof.Init());
  prof.SetStatsResetIntervalMs(0);

  const u32 hash = ::gecko::FNV1a("Orphan");
  prof.Emit(MakeZoneEnd(hash, 1000, ProfLevel::Always));

  ScopeStats s = prof.GetStats(hash);
  REQUIRE(s.Count == 0u);

  prof.Shutdown();
}

TEST_CASE("RingProfiler aggregator: orphan ZoneBegin does not produce a sample", "[runtime][profiler][aggregator]")
{
  // Begin without matching End leaves the open-scope stack non-empty for
  // this thread but contributes no Count and no Min/Max update.
  RingProfiler prof(64);
  REQUIRE(prof.Init());
  prof.SetStatsResetIntervalMs(0);

  const u32 hash = ::gecko::FNV1a("OnlyBegin");
  prof.Emit(MakeZoneBegin(hash, 1000, ProfLevel::Always));

  ScopeStats s = prof.GetStats(hash);
  REQUIRE(s.Count == 0u);

  // A subsequent matching End closes the dangling Begin and produces one
  // sample, confirming the open-scope frame survived correctly.
  prof.Emit(MakeZoneEnd(hash, 1500, ProfLevel::Always));
  s = prof.GetStats(hash);
  REQUIRE(s.Count == 1u);
  REQUIRE(s.LastNs == 500u);

  prof.Shutdown();
}
