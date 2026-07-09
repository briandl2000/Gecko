#include "gecko/core/utility/format.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

namespace {

class CountingAllocator final : public IAllocator
{
public:
  void* Alloc(u64 size, u32 alignment) noexcept override
  {
    void* raw = PlatformAlloc(TotalAllocSize(size, alignment), EffectiveAlignment(alignment));
    if (raw == nullptr)
      return nullptr;
    ++Allocs;
    LiveBytes += size;
    return PlaceAllocHeader(raw, size, alignment, SystemAllocMagic, CurrentLabel(), this);
  }

  void Free(void* ptr) noexcept override
  {
    if (ptr == nullptr)
      return;
    auto* header = HeaderFromUserPtr(ptr);
    if (!IsAllocHeaderValid(header))
    {
      ++InvalidFrees;
      return;
    }
    LiveBytes -= header->RequestedSize;
    ++Frees;
    void* raw = RawPtrFromHeader(header);
    const u32 alignment = header->Alignment;
    header->Magic = 0;
    PlatformFree(raw, alignment);
  }

  void PushLabel(Label) noexcept override
  {}
  void PopLabel() noexcept override
  {}
  Label CurrentLabel() const noexcept override
  {
    return {};
  }
  bool Init() noexcept override
  {
    return true;
  }
  void Shutdown() noexcept override
  {}

  u32 Allocs {0};
  u32 Frees {0};
  u32 InvalidFrees {0};
  u64 LiveBytes {0};
};

}  // namespace

TEST_CASE("Format returns Gecko String using current allocator", "[core][format]")
{
  CountingAllocator alloc;
  String text;

  {
    AllocatorPushScope scope {alloc};
    text = Format("hello {}", 42);
  }

  REQUIRE(text.View() == "hello 42");
  REQUIRE(text.Allocator() == &alloc);

  text.Reset();
  REQUIRE(alloc.InvalidFrees == 0);
  REQUIRE(alloc.Frees == alloc.Allocs);
  REQUIRE(alloc.LiveBytes == 0);
}

TEST_CASE("Format supports explicit allocator", "[core][format]")
{
  CountingAllocator alloc;
  String text = Format(&alloc, "{} {}", "gecko", "engine");

  REQUIRE(text.View() == "gecko engine");
  REQUIRE(text.Allocator() == &alloc);

  text.Reset();
  REQUIRE(alloc.InvalidFrees == 0);
  REQUIRE(alloc.Frees == alloc.Allocs);
  REQUIRE(alloc.LiveBytes == 0);
}

TEST_CASE("RuntimeFormat and AppendFormat append to existing String", "[core][format]")
{
  CountingAllocator alloc;
  String text {alloc, "value"};

  REQUIRE(AppendFormat(text, RuntimeFormat("={:02}"), 7));
  REQUIRE(text.View() == "value=07");
  REQUIRE(text.Allocator() == &alloc);
}
