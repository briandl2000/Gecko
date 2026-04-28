#include "gecko/core/services/memory.h"

#include "gecko/core/types.h"

#include <cstdlib>
#if defined(GECKO_PLATFORM_WINDOWS)
#include <malloc.h>
#endif

namespace gecko {

// ------------------------------------------------------------
// Platform Allocation
// ------------------------------------------------------------

void* PlatformAlloc(u64 size, u32 alignment) noexcept
{
  if (alignment <= alignof(::std::max_align_t))
  {
    return ::std::malloc(static_cast<usize>(size));
  }

#if defined(GECKO_PLATFORM_WINDOWS)
  return ::_aligned_malloc(static_cast<usize>(size), alignment);
#else
  void* ptr = nullptr;
  if (::posix_memalign(&ptr, alignment, static_cast<usize>(size)) != 0)
    return nullptr;
  return ptr;
#endif
}

void PlatformFree(void* ptr, u32 alignment) noexcept
{
  if (!ptr)
    return;

#if defined(GECKO_PLATFORM_WINDOWS)
  if (alignment > alignof(::std::max_align_t))
    ::_aligned_free(ptr);
  else
    ::std::free(ptr);
#else
  // posix_memalign returns memory freeable with free()
  ::std::free(ptr);
#endif
}

// ------------------------------------------------------------
// SystemAllocator
// ------------------------------------------------------------

void* SystemAllocator::Alloc(u64 size, u32 alignment) noexcept
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  const u32 effAlign = EffectiveAlignment(alignment);
  const u64 totalSize = TotalAllocSize(size, alignment);

  void* rawPtr = PlatformAlloc(totalSize, effAlign);
  if (!rawPtr)
    return nullptr;

  return PlaceAllocHeader(rawPtr, size, alignment, SystemAllocMagic);
}

void SystemAllocator::Free(void* ptr) noexcept
{
  if (!ptr)
    return;

  auto* header = HeaderFromUserPtr(ptr);

  if (header->Magic == SystemAllocMagic || header->Magic == TrackingAllocMagic)
  {
    void* rawPtr = RawPtrFromHeader(header);
    u32 alignment = header->Alignment;
    header->Magic = 0;  // Poison against double-free
    PlatformFree(rawPtr, alignment);
    return;
  }

  GECKO_ASSERT(false && "SystemAllocator::Free: unknown allocation or "
                        "double-free");
}

bool SystemAllocator::Init() noexcept
{
  return true;
}

void SystemAllocator::Shutdown() noexcept
{}

}  // namespace gecko
