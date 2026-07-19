#pragma once

#include "gecko/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

namespace gecko {

struct MemoryStats
{
  u64 ReservedBytes {0};
  u64 CommittedBytes {0};
  u64 LiveBytes {0};
  u64 PeakBytes {0};
  u64 AllocationCount {0};
};

[[nodiscard]] GECKO_API void* AllocBytes(u64 size, u32 alignment = alignof(long double)) noexcept;
GECKO_API void DeallocBytes(void* memory) noexcept;
[[nodiscard]] GECKO_API MemoryStats GetMemoryStats() noexcept;

GECKO_API void PushMemoryLabel(Label label) noexcept;
GECKO_API void PopMemoryLabel() noexcept;
[[nodiscard]] GECKO_API Label CurrentMemoryLabel() noexcept;

template <typename T>
[[nodiscard]] T* AllocArray(u64 count, u32 alignment = alignof(T)) noexcept
{
  GECKO_ASSERT(count != 0, "Cannot allocate zero elements");
  GECKO_ASSERT(count <= USizeMax / sizeof(T), "Allocation size overflow");
  return static_cast<T*>(AllocBytes(sizeof(T) * count, alignment));
}

struct MemoryLabelScope
{
  explicit MemoryLabelScope(Label label) noexcept
  {
    PushMemoryLabel(label);
  }

  ~MemoryLabelScope() noexcept
  {
    PopMemoryLabel();
  }

  MemoryLabelScope(const MemoryLabelScope&) = delete;
  MemoryLabelScope& operator=(const MemoryLabelScope&) = delete;
};

#define GECKO_MEMORY_CONCAT_INNER(a, b) a##b
#define GECKO_MEMORY_CONCAT(a, b) GECKO_MEMORY_CONCAT_INNER(a, b)
#define GECKO_PUSH_LABEL(label)                                              \
  gecko::MemoryLabelScope GECKO_MEMORY_CONCAT(g_MemoryLabelScope_, __LINE__) \
  {                                                                          \
    (label)                                                                  \
  }

}  // namespace gecko
