#pragma once
#include <cstddef>

#include "api.h"
#include "category.h"
#include "types.h"

namespace gecko {

  struct IAllocator 
  {
    GECKO_API virtual ~IAllocator() = default;

    GECKO_API virtual void* alloc(u64 size, u32 alignment, Category category) noexcept = 0;

    GECKO_API virtual void free(void* ptr, u64 size, u32 alignment, Category category) noexcept = 0;
  };

  IAllocator* GetAllocator() noexcept;

  [[nodiscard]]
  GECKO_API inline void* AllocBytes(u64 size, u32 alignment, Category category) noexcept 
  {
    return GetAllocator()->alloc(size, alignment, category);
  }

  GECKO_API inline void DeallocBytes(void* ptr, u64 size, u32 alignment, Category category) noexcept
  {
    GetAllocator()->free(ptr, size, alignment, category);
  }

  template<class T>
  [[nodiscard]]
  GECKO_API inline T* AllocArray(u64 count, Category category, u32 alignment = alignof(T)) noexcept
  {
    return static_cast<T*>(AllocBytes(sizeof(T) * count, alignment, category));
  }

}
