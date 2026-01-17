#include "gecko/core/services/memory.h"

#include "gecko/core/types.h"

#include <cstdlib>

namespace gecko {

void* SystemAllocator::Alloc(u64 size, u32 alignment, Label label) noexcept
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  (void)label;

  if (alignment <= alignof(std::max_align_t))
  {
    return ::std::malloc(static_cast<usize>(size));
  }

#if defined(_MSC_VER)
  return ::_aligned_malloc(static_cast<usize>(size), alignment);
#else
  void* ptr = nullptr;
  if (::posix_memalign(&ptr, alignment, static_cast<usize>(size)) != 0)
    return nullptr;
  return ptr;
#endif
}

void SystemAllocator::Free(void* ptr, u64 size, u32 alignment,
                           Label label) noexcept
{
  if (!ptr)
    return;

  (void)size;
  (void)label;
#if defined(_MSC_VER)
  if (alignment > alignof(::std::max_align_t))
  {
    ::_aligned_free(ptr);
    return;
  }
#endif
  ::std::free(ptr);
}
bool SystemAllocator::Init() noexcept
{
  return true;
}
void SystemAllocator::Shutdown() noexcept
{}
}  // namespace gecko
