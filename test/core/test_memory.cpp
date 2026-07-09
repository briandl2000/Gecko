#include "gecko/core/services/memory.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace gecko;

TEST_CASE("SystemAllocator Init/Shutdown", "[core][memory]")
{
  SystemAllocator alloc;
  REQUIRE(alloc.Init());
  alloc.Shutdown();
}

TEST_CASE("SystemAllocator basic alloc/free", "[core][memory]")
{
  SystemAllocator alloc;
  alloc.Init();

  void* ptr = alloc.Alloc(128, alignof(::std::max_align_t));
  REQUIRE(ptr != nullptr);

  ::std::memset(ptr, 0xAB, 128);
  alloc.Free(ptr);

  alloc.Shutdown();
}

TEST_CASE("SystemAllocator aligned allocation", "[core][memory]")
{
  SystemAllocator alloc;
  alloc.Init();

  for (u32 alignment : {16u, 32u, 64u, 128u})
  {
    void* ptr = alloc.Alloc(256, alignment);
    REQUIRE(ptr != nullptr);
    REQUIRE(reinterpret_cast<uintptr_t>(ptr) % alignment == 0);
    alloc.Free(ptr);
  }

  alloc.Shutdown();
}

TEST_CASE("AllocHeader roundtrip", "[core][memory]")
{
  void* raw = PlatformAlloc(TotalAllocSize(64, 16), EffectiveAlignment(16));
  REQUIRE(raw != nullptr);

  SystemAllocator owner;
  void* user = PlaceAllocHeader(raw, 64, 16, SystemAllocMagic, {}, &owner);
  REQUIRE(user != nullptr);

  AllocHeader* header = HeaderFromUserPtr(user);
  REQUIRE(header != nullptr);
  REQUIRE(IsAllocHeaderValid(header));
  REQUIRE(header->Magic == SystemAllocMagic);
  REQUIRE(header->RequestedSize == 64);
  REQUIRE(header->Owner == &owner);

  void* recovered = RawPtrFromHeader(header);
  REQUIRE(recovered == raw);

  PlatformFree(raw, EffectiveAlignment(16));
}

TEST_CASE("PlatformAlloc/PlatformFree smoke", "[core][memory]")
{
  void* ptr = PlatformAlloc(512, 64);
  REQUIRE(ptr != nullptr);
  REQUIRE(reinterpret_cast<uintptr_t>(ptr) % 64 == 0);
  PlatformFree(ptr, 64);
}

TEST_CASE("TotalAllocSize accounts for header and alignment", "[core][memory]")
{
  u64 total = TotalAllocSize(100, 16);
  REQUIRE(total >= 100 + sizeof(AllocHeader));
}

TEST_CASE("EffectiveAlignment clamps to header alignment", "[core][memory]")
{
  REQUIRE(EffectiveAlignment(1) >= alignof(AllocHeader));
  REQUIRE(EffectiveAlignment(128) == 128);
}

TEST_CASE("SystemAllocator label ops are no-ops", "[core][memory]")
{
  SystemAllocator alloc;
  alloc.Init();

  Label testLabel = MakeLabel("test");
  alloc.PushLabel(testLabel);
  REQUIRE_FALSE(alloc.CurrentLabel().IsValid());
  alloc.PopLabel();

  alloc.Shutdown();
}
