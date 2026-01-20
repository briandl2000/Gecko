#if GECKO_OVERRIDE_NEW
#include "gecko/core/assert.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/types.h"

#include <cstddef>
#include <new>

void* operator new(::gecko::usize size)
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");

  auto* allocator = ::gecko::GetAllocator();
  GECKO_ASSERT(allocator &&
               "Allocation before GECKO_BOOT or after UninstallServices()");

  if (void* ptr = allocator->Alloc(static_cast<::gecko::u64>(size),
                                   alignof(::std::max_align_t)))
  {
    return ptr;
  }

  throw ::std::bad_alloc {};
}

void operator delete(void* ptr) noexcept
{
  if (!ptr)
    return;

  auto* allocator = ::gecko::GetAllocator();
  GECKO_ASSERT(allocator &&
               "Deallocation after UninstallServices() - memory leak");

  allocator->Free(ptr);
}

void* operator new(::gecko::usize size, ::std::align_val_t align)
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");

  auto* allocator = ::gecko::GetAllocator();
  GECKO_ASSERT(allocator &&
               "Allocation before GECKO_BOOT or after UninstallServices()");

  if (void* ptr = allocator->Alloc(static_cast<::gecko::u64>(size),
                                   static_cast<::gecko::u32>(align)))
  {
    return ptr;
  }

  throw ::std::bad_alloc {};
}

void operator delete(void* ptr, ::std::align_val_t) noexcept
{
  if (!ptr)
    return;

  auto* allocator = ::gecko::GetAllocator();
  GECKO_ASSERT(allocator &&
               "Deallocation after UninstallServices() - memory leak");

  allocator->Free(ptr);
}

void operator delete(void* ptr, ::gecko::usize) noexcept
{
  if (!ptr)
    return;

  auto* allocator = ::gecko::GetAllocator();
  GECKO_ASSERT(allocator &&
               "Deallocation after UninstallServices() - memory leak");

  allocator->Free(ptr);
}

void operator delete(void* ptr, ::gecko::usize, ::std::align_val_t) noexcept
{
  if (!ptr)
    return;

  auto* allocator = ::gecko::GetAllocator();
  GECKO_ASSERT(allocator &&
               "Deallocation after UninstallServices() - memory leak");

  allocator->Free(ptr);
}

#endif
