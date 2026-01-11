#if GECKO_OVERRIDE_NEW
#include <cstddef>
#include <new>

#include "gecko/core/assert.h"
#include "gecko/core/memory.h"

#include "labels.h"

void *operator new(std::size_t size) {
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");

  if (auto *allocator = gecko::GetAllocator()) {
    if (void *ptr = allocator->Alloc(static_cast<gecko::u64>(size),
                                     alignof(std::max_align_t),
                                     gecko::runtime::labels::OperatorNew)) {
      return ptr;
    }
  }
  throw std::bad_alloc{};
}

void operator delete(void *ptr) noexcept {
  if (!ptr) {
    return;
  }
  if (auto *allocator = gecko::GetAllocator()) {
    allocator->Free(ptr, 0, alignof(std::max_align_t),
                    gecko::runtime::labels::OperatorNew);
  }
}

void *operator new(std::size_t size, std::align_val_t align) {
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(static_cast<gecko::u64>(align) > 0 &&
               "Alignment must be greater than 0");

  if (auto *allocator = gecko::GetAllocator()) {
    if (void *ptr = allocator->Alloc(static_cast<gecko::u64>(size),
                                     static_cast<gecko::u64>(align),
                                     gecko::runtime::labels::OperatorNew)) {
      return ptr;
    }
  }
  throw std::bad_alloc{};
}

void operator delete(void *ptr, std::align_val_t align) noexcept {
  if (!ptr) {
    return;
  }
  if (auto *allocator = gecko::GetAllocator()) {
    allocator->Free(ptr, 0, static_cast<gecko::u64>(align),
                    gecko::runtime::labels::OperatorNew);
  }
}

void operator delete(void *ptr, std::size_t size) noexcept {
  if (!ptr) {
    return;
  }
  if (auto *allocator = gecko::GetAllocator()) {
    allocator->Free(ptr, size, alignof(std::max_align_t),
                    gecko::runtime::labels::OperatorNew);
  }
}

#endif
