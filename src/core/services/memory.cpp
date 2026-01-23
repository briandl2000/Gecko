#include "gecko/core/services/memory.h"

#include "gecko/core/types.h"

#include <cstdlib>

namespace gecko {

void* SystemAllocator::Alloc(u64 size, u32 alignment) noexcept
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  // For small alignments, just use malloc
  if (alignment <= alignof(std::max_align_t))
  {
    return ::std::malloc(static_cast<usize>(size));
  }

  // For larger alignments, use platform-specific aligned allocation
#if defined(_MSC_VER)
  return ::_aligned_malloc(static_cast<usize>(size), alignment);
#else
  void* ptr = nullptr;
  if (::posix_memalign(&ptr, alignment, static_cast<usize>(size)) != 0)
    return nullptr;
  return ptr;
#endif
}

void SystemAllocator::Free(void* ptr) noexcept
{
  if (!ptr)
    return;

  ::std::free(ptr);
}

bool SystemAllocator::Init() noexcept
{
  return true;
}

void SystemAllocator::Shutdown() noexcept
{}

}  // namespace gecko
