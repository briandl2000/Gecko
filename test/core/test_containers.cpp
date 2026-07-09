#include "gecko/core/array.h"
#include "gecko/core/string.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

namespace {

class CountingAllocator final : public IAllocator
{
public:
  void* Alloc(u64 size, u32 alignment) noexcept override
  {
    const u32 effAlign = EffectiveAlignment(alignment);
    void* raw = PlatformAlloc(TotalAllocSize(size, alignment), effAlign);
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
      InvalidFrees += 1;
      return;
    }

    LiveBytes -= header->RequestedSize;
    ++Frees;

    void* raw = RawPtrFromHeader(header);
    const u32 alignment = header->Alignment;
    header->Magic = 0;
    PlatformFree(raw, alignment);
  }

  void PushLabel(Label label) noexcept override
  {
    m_Label = label;
  }

  void PopLabel() noexcept override
  {
    m_Label = {};
  }

  Label CurrentLabel() const noexcept override
  {
    return m_Label;
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

private:
  Label m_Label {};
};

struct LifetimeProbe
{
  static inline int Live = 0;

  LifetimeProbe() noexcept
  {
    ++Live;
  }

  LifetimeProbe(const LifetimeProbe&) noexcept
  {
    ++Live;
  }

  LifetimeProbe(LifetimeProbe&&) noexcept
  {
    ++Live;
  }

  ~LifetimeProbe() noexcept
  {
    --Live;
  }
};

}  // namespace

TEST_CASE("AllocatorPushScope changes CurrentAllocator on this thread", "[core][containers][allocator]")
{
  CountingAllocator alloc;

  IAllocator* before = &CurrentAllocator();
  {
    AllocatorPushScope scope {alloc};
    REQUIRE(scope.Ok());
    REQUIRE(&CurrentAllocator() == &alloc);
  }
  REQUIRE(&CurrentAllocator() == before);
}

TEST_CASE("Array stores allocator used at creation", "[core][containers][array]")
{
  CountingAllocator alloc;
  Array<int> values;

  {
    AllocatorPushScope scope {alloc};
    values = Array<int> {};
    REQUIRE(values.Allocator() == &alloc);
    REQUIRE(values.PushBack(7));
    REQUIRE(values.PushBack(11));
  }

  REQUIRE(values.Count() == 2);
  REQUIRE(values[0] == 7);
  REQUIRE(values[1] == 11);
  REQUIRE(alloc.Allocs > 0);
  REQUIRE(alloc.Frees == 0);

  values.Reset();
  REQUIRE(alloc.InvalidFrees == 0);
  REQUIRE(alloc.Frees == alloc.Allocs);
  REQUIRE(alloc.LiveBytes == 0);
}

TEST_CASE("DeallocBytes routes to allocation owner after allocator scope ends", "[core][containers][allocator]")
{
  CountingAllocator alloc;
  void* ptr = nullptr;

  {
    AllocatorPushScope scope {alloc};
    ptr = AllocBytes(64, 16);
  }

  REQUIRE(ptr != nullptr);
  REQUIRE(alloc.Allocs == 1);
  REQUIRE(alloc.Frees == 0);

  DeallocBytes(ptr);
  REQUIRE(alloc.InvalidFrees == 0);
  REQUIRE(alloc.Frees == 1);
  REQUIRE(alloc.LiveBytes == 0);
}

TEST_CASE("Array destroys non-trivial elements", "[core][containers][array]")
{
  LifetimeProbe::Live = 0;

  {
    Array<LifetimeProbe> values;
    REQUIRE(values.EmplaceBack() != nullptr);
    REQUIRE(values.EmplaceBack() != nullptr);
    REQUIRE(LifetimeProbe::Live == 2);
  }

  REQUIRE(LifetimeProbe::Live == 0);
}

TEST_CASE("String stores allocator and remains null-terminated", "[core][containers][string]")
{
  CountingAllocator alloc;
  String text;

  {
    AllocatorPushScope scope {alloc};
    text = String {"gecko"};
    REQUIRE(text.Allocator() == &alloc);
  }

  REQUIRE(text.Size() == 5);
  REQUIRE(text.View() == "gecko");
  REQUIRE(text.CStr()[text.Size()] == '\0');
  REQUIRE(text.Append(" engine"));
  REQUIRE(text.View() == "gecko engine");
  REQUIRE(text.CStr()[text.Size()] == '\0');

  text.Reset();
  REQUIRE(alloc.InvalidFrees == 0);
  REQUIRE(alloc.Frees == alloc.Allocs);
  REQUIRE(alloc.LiveBytes == 0);
}
