#pragma once

#include <climits>
#include <cstddef>

#include "api.h"
#include "assert.h"
#include "category.h"
#include "types.h"

namespace gecko {

struct IAllocator {
  GECKO_API virtual ~IAllocator() = default;

  GECKO_API virtual void *Alloc(u64 size, u32 alignment,
                                Category category) noexcept = 0;

  GECKO_API virtual void Free(void *ptr, u64 size, u32 alignment,
                              Category category) noexcept = 0;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;
};

GECKO_API IAllocator *GetAllocator() noexcept;

[[nodiscard]]
GECKO_API inline void *AllocBytes(u64 size, u32 alignment,
                                  Category category) noexcept {
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");
  if (auto *allocator = GetAllocator())
    return allocator->Alloc(size, alignment, category);
  return nullptr;
}

GECKO_API inline void DeallocBytes(void *ptr, u64 size, u32 alignment,
                                   Category category) noexcept {
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");
  if (auto *allocator = GetAllocator())
    allocator->Free(ptr, size, alignment, category);
}

template <class T>
[[nodiscard]]
GECKO_API inline T *AllocArray(u64 count, Category category,
                               u32 alignment = alignof(T)) noexcept {
  GECKO_ASSERT(count > 0 && "Cannot allocate zero elements");
  GECKO_ASSERT(count <= (SIZE_MAX / sizeof(T)) && "Count would overflow");
  return static_cast<T *>(AllocBytes(sizeof(T) * count, alignment, category));
}

} // namespace gecko
