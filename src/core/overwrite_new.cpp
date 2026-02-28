#if GECKO_OVERRIDE_NEW
#include "gecko/core/services/memory.h"

#include <cstddef>
#include <new>

void* operator new(::std::size_t size)
{
  return ::gecko::AllocBytes(static_cast<::gecko::u64>(size));
}

void* operator new(::std::size_t size, ::std::align_val_t align)
{
  return ::gecko::AllocBytes(static_cast<::gecko::u64>(size),
                             static_cast<::gecko::u32>(align));
}

void* operator new(::std::size_t size, const ::std::nothrow_t&) noexcept
{
  return ::gecko::AllocBytes(static_cast<::gecko::u64>(size));
}

void operator delete(void* ptr) noexcept
{
  ::gecko::DeallocBytes(ptr);
}

void operator delete(void* ptr, ::std::size_t) noexcept
{
  ::gecko::DeallocBytes(ptr);
}

void operator delete(void* ptr, ::std::align_val_t) noexcept
{
  ::gecko::DeallocBytes(ptr);
}
#endif
