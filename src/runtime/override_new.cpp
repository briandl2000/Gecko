#include "gecko/core/profiler.h"

#if GECKO_OVERRIDE_NEW
#include <cstddef>
#include <new>

#include "gecko/core/assert.h"
#include "gecko/core/core.h"

#include "categories.h"

void* operator new(std::size_t size) 
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  
  if (auto* Allocator = gecko::GetAllocator())
  {
    if (void* ptr = Allocator->Alloc(static_cast<gecko::u64>(size), alignof(std::max_align_t), gecko::runtime::categories::OperatorNew))
    {
      return ptr;
    }
  }
  throw std::bad_alloc{};
}

void operator delete(void* ptr) noexcept
{
  if (!ptr) return;
  if (auto* Allocator = gecko::GetAllocator())
  {
    Allocator->Free(ptr, 0, alignof(std::max_align_t), gecko::runtime::categories::OperatorNew);
  }
}

void* operator new(std::size_t size, std::align_val_t align)
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(static_cast<gecko::u64>(align) > 0 && "Alignment must be greater than 0");
  
  if (auto* Allocator = gecko::GetAllocator())
  {
    if (void* ptr = Allocator->Alloc(static_cast<gecko::u64>(size), static_cast<gecko::u64>(align), gecko::runtime::categories::OperatorNew))
    {
      return ptr;
    }
  }
  throw std::bad_alloc{};
}

void operator delete(void* ptr, std::align_val_t align) noexcept 
{
  if (!ptr) return; 
  if (auto* Allocator = gecko::GetAllocator())
  {
    Allocator->Free(ptr, 0, static_cast<gecko::u64>(align), gecko::runtime::categories::OperatorNew);
  }
}

#endif
