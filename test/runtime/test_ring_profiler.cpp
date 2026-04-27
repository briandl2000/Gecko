#include "gecko/core/services/profiler.h"
#include "gecko/core/utility/hash.h"
#include "gecko/runtime/ring_profiler.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace gecko;
using namespace gecko::runtime;

namespace {

ProfEvent MakeZoneBegin(u32 hash, u64 t,
                        ProfLevel lvl = ProfLevel::Normal) noexcept
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

ProfEvent MakeZoneEnd(u32 hash, u64 t,
                      ProfLevel lvl = ProfLevel::Normal) noexcept
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

TEST_CASE("RingProfiler FIFO ordering via TryPop", "[runtime][profiler]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  for (u64 i = 0; i < 10; ++i)
    prof.Emit(MakeZoneBegin(0xAAAA, i + 1));

  ProfEvent ev {};
  for (u64 i = 0; i < 10; ++i)
  {
    REQUIRE(prof.TryPop(ev));
    REQUIRE(ev.TimestampNs == i + 1);
    REQUIRE(ev.NameHash == 0xAAAAu);
  }
  REQUIRE_FALSE(prof.TryPop(ev));

  prof.Shutdown();
}

TEST_CASE("RingProfiler overflow drops events", "[runtime][profiler]")
{
  RingProfiler prof(8);  // tiny ring
  REQUIRE(prof.Init());

  // Without consuming, push more than capacity.
  for (u64 i = 0; i < 64; ++i)
    prof.Emit(MakeZoneBegin(0x1, i + 1));

  ProfilerDiagnostics diag = prof.GetDiagnostics();
  REQUIRE(diag.DroppedEvents > 0);

  prof.Shutdown();
}

TEST_CASE("RingProfiler aggregator min/max/last/count",
          "[runtime][profiler][aggregator]")
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

TEST_CASE("RingProfiler aggregator captures every level",
          "[runtime][profiler][aggregator]")
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

TEST_CASE("RingProfiler FrameMark no longer auto-resets aggregator",
          "[runtime][profiler][aggregator]")
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

TEST_CASE("RingProfiler categories: register, enable, disable",
          "[runtime][profiler][categories]")
{
  RingProfiler prof(64);
  REQUIRE(prof.Init());

  // Default category 0 is enabled.
  REQUIRE(prof.IsCategoryEnabled(0));

  u8 cat = prof.RegisterCategory("net");
  REQUIRE(cat != 0);
  REQUIRE(cat != c_ProfInvalidCategory);
  REQUIRE(prof.IsCategoryEnabled(cat));

  // Same name -> same id.
  REQUIRE(prof.RegisterCategory("net") == cat);

  prof.SetCategoryEnabled(cat, false);
  REQUIRE_FALSE(prof.IsCategoryEnabled(cat));

  prof.SetCategoryEnabled(cat, true);
  REQUIRE(prof.IsCategoryEnabled(cat));

  prof.Shutdown();
}

TEST_CASE("RingProfiler diagnostics counters non-decreasing",
          "[runtime][profiler]")
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
