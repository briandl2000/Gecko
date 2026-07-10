#include "gecko/core/labels.h"
#include "gecko/runtime/tracking_allocator.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace gecko;
using namespace gecko::runtime;

TEST_CASE("TrackingAllocator Init/Shutdown", "[runtime][allocator]")
{
  TrackingAllocator alloc;
  REQUIRE(alloc.Init());
  REQUIRE(alloc.TotalLiveBytes() == 0);
  alloc.Shutdown();
}

TEST_CASE("TrackingAllocator basic alloc/free", "[runtime][allocator]")
{
  TrackingAllocator alloc;
  alloc.Init();

  void* ptr = alloc.Alloc(256, alignof(::std::max_align_t));
  REQUIRE(ptr != nullptr);
  REQUIRE(alloc.TotalLiveBytes() >= 256);

  ::std::memset(ptr, 0xCD, 256);
  alloc.Free(ptr);
  REQUIRE(alloc.TotalLiveBytes() == 0);

  alloc.Shutdown();
}

TEST_CASE("TrackingAllocator aligned allocation", "[runtime][allocator]")
{
  TrackingAllocator alloc;
  alloc.Init();

  for (u32 alignment : {16u, 32u, 64u, 128u, 256u})
  {
    void* ptr = alloc.Alloc(512, alignment);
    REQUIRE(ptr != nullptr);
    REQUIRE(reinterpret_cast<uintptr_t>(ptr) % alignment == 0);
    alloc.Free(ptr);
  }

  alloc.Shutdown();
}

TEST_CASE("TrackingAllocator label tracking", "[runtime][allocator]")
{
  TrackingAllocator alloc;
  alloc.Init();

  Label textures = MakeLabel("textures");
  Label meshes = MakeLabel("meshes");

  alloc.PushLabel(textures);
  void* t1 = alloc.Alloc(1024, 16);
  void* t2 = alloc.Alloc(2048, 16);
  alloc.PopLabel();

  alloc.PushLabel(meshes);
  void* m1 = alloc.Alloc(512, 16);
  alloc.PopLabel();

  MemLabelStats texStats;
  REQUIRE(alloc.StatsFor(textures, texStats));
  REQUIRE(texStats.Allocs == 2);
  REQUIRE(texStats.LiveBytes >= 1024 + 2048);

  MemLabelStats meshStats;
  REQUIRE(alloc.StatsFor(meshes, meshStats));
  REQUIRE(meshStats.Allocs == 1);
  REQUIRE(meshStats.LiveBytes >= 512);

  alloc.Free(t1);
  alloc.Free(t2);
  alloc.Free(m1);

  REQUIRE(alloc.TotalLiveBytes() == 0);
  alloc.Shutdown();
}

TEST_CASE("TrackingAllocator multiple alloc/free cycles", "[runtime][allocator]")
{
  TrackingAllocator alloc;
  alloc.Init();

  for (int i = 0; i < 100; ++i)
  {
    void* ptr = alloc.Alloc(64, 16);
    REQUIRE(ptr != nullptr);
    alloc.Free(ptr);
  }

  REQUIRE(alloc.TotalLiveBytes() == 0);
  alloc.Shutdown();
}

TEST_CASE("TrackingAllocator snapshot", "[runtime][allocator]")
{
  TrackingAllocator alloc;
  alloc.Init();

  Label audio = MakeLabel("audio");
  alloc.PushLabel(audio);
  void* ptr = alloc.Alloc(128, 16);
  alloc.PopLabel();

  auto snapshot = alloc.Snapshot();
  bool found = false;
  for (const MemLabelStats& stats : snapshot)
    found = found || stats.StatsLabel.Id == audio.Id;
  REQUIRE(found);

  alloc.Free(ptr);
  alloc.Shutdown();
}

TEST_CASE("TrackingAllocator label stack", "[runtime][allocator]")
{
  TrackingAllocator alloc;
  alloc.Init();

  Label outer = MakeLabel("outer");
  Label inner = MakeLabel("inner");

  alloc.PushLabel(outer);
  REQUIRE(alloc.CurrentLabel() == outer);

  alloc.PushLabel(inner);
  REQUIRE(alloc.CurrentLabel() == inner);

  alloc.PopLabel();
  REQUIRE(alloc.CurrentLabel() == outer);

  alloc.PopLabel();
  REQUIRE(alloc.CurrentLabel() != outer);
  REQUIRE(alloc.CurrentLabel() != inner);

  alloc.Shutdown();
}
