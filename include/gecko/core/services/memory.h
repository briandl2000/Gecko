#pragma once

#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

namespace gecko {

struct LabelScope;

struct IAllocator
{
  GECKO_API virtual ~IAllocator() = default;

  // Core allocation methods
  GECKO_API virtual void* Alloc(u64 size, u32 alignment) noexcept = 0;
  GECKO_API virtual void Free(void* ptr) noexcept = 0;

  // Label stack for allocation tracking.
  // These are public for interface compatibility but should only be called
  // via LabelScope (GECKO_PUSH_LABEL macro) to ensure proper pairing.
  GECKO_API virtual void PushLabel(Label label) noexcept = 0;
  GECKO_API virtual void PopLabel() noexcept = 0;
  GECKO_API virtual Label CurrentLabel() const noexcept = 0;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;
};

GECKO_API IAllocator* GetAllocator() noexcept;

[[nodiscard]]
inline void* AllocBytes(u64 size,
                        u32 alignment = alignof(std::max_align_t)) noexcept
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");
  if (auto* allocator = GetAllocator())
    return allocator->Alloc(size, alignment);
  return nullptr;
}

inline void DeallocBytes(void* ptr) noexcept
{
  if (auto* allocator = GetAllocator())
    allocator->Free(ptr);
}

template <class T>
[[nodiscard]]
inline T* AllocArray(u64 count, u32 alignment = alignof(T)) noexcept
{
  GECKO_ASSERT(count > 0 && "Cannot allocate zero elements");
  GECKO_ASSERT(count <= (SIZE_MAX / sizeof(T)) && "Count would overflow");
  return static_cast<T*>(AllocBytes(sizeof(T) * count, alignment));
}

struct LabelScope
{
  LabelScope(Label label) noexcept
  {
    if (auto* alloc = GetAllocator())
      alloc->PushLabel(label);
  }
  ~LabelScope() noexcept
  {
    if (auto* alloc = GetAllocator())
      alloc->PopLabel();
  }
  LabelScope(const LabelScope&) = delete;
  LabelScope& operator=(const LabelScope&) = delete;
};

#define GECKO_PUSH_LABEL(label)      \
  ::gecko::LabelScope _g_label_scope \
  {                                  \
    (label)                          \
  }

struct SystemAllocator final : IAllocator
{
  GECKO_API void* Alloc(u64 size, u32 alignment) noexcept override;
  GECKO_API void Free(void* ptr) noexcept override;

  // No-op label tracking - SystemAllocator doesn't track allocations
  GECKO_API void PushLabel(Label) noexcept override
  {}
  GECKO_API void PopLabel() noexcept override
  {}
  GECKO_API Label CurrentLabel() const noexcept override
  {
    return {};
  }

  GECKO_API bool Init() noexcept override;
  GECKO_API void Shutdown() noexcept override;
};

}  // namespace gecko
