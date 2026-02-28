#pragma once

#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

#include <cstdint>

namespace gecko {

struct LabelScope;

//------------------------------------------------------------
// Allocation Header
//------------------------------------------------------------
// All IAllocator implementations prepend this header. The Magic field enables
// safe cross-allocator frees (e.g., pre-boot allocations freed post-boot).

constexpr u32 SystemAllocMagic = 0x53595341;    // "SYSA"
constexpr u32 TrackingAllocMagic = 0x47454B4F;  // "GEKO"

struct AllocHeader
{
  u32 Magic;
  u32 Alignment;
  u64 RequestedSize;
  Label AllocLabel;
  u64 RawOffset;  // Byte offset from this header back to the raw platform
                  // pointer
};

inline AllocHeader* HeaderFromUserPtr(void* userPtr) noexcept
{
  return reinterpret_cast<AllocHeader*>(static_cast<u8*>(userPtr) -
                                        sizeof(AllocHeader));
}

inline void* RawPtrFromHeader(AllocHeader* header) noexcept
{
  return reinterpret_cast<u8*>(header) - header->RawOffset;
}

inline bool IsAllocHeaderValid(const AllocHeader* header) noexcept
{
  return header && (header->Magic == SystemAllocMagic ||
                    header->Magic == TrackingAllocMagic);
}

//------------------------------------------------------------
// Platform Allocation
//------------------------------------------------------------

GECKO_API void* PlatformAlloc(u64 size, u32 alignment) noexcept;
GECKO_API void PlatformFree(void* ptr, u32 alignment) noexcept;

//------------------------------------------------------------
// Allocation Header Helpers
//------------------------------------------------------------

inline u32 EffectiveAlignment(u32 userAlignment) noexcept
{
  return userAlignment > alignof(AllocHeader)
             ? userAlignment
             : static_cast<u32>(alignof(AllocHeader));
}

// rawPtr must come from PlatformAlloc(TotalAllocSize(...),
// EffectiveAlignment(...)).
inline void* PlaceAllocHeader(void* rawPtr, u64 size, u32 userAlignment,
                              u32 magic, Label label = {}) noexcept
{
  const u32 effAlign = EffectiveAlignment(userAlignment);
  auto rawAddr = reinterpret_cast<uintptr_t>(rawPtr);

  const uintptr_t alignMask = static_cast<uintptr_t>(effAlign) - 1;
  uintptr_t userAddr = (rawAddr + sizeof(AllocHeader) + alignMask) & ~alignMask;

  auto* header = reinterpret_cast<AllocHeader*>(userAddr - sizeof(AllocHeader));
  header->Magic = magic;
  header->Alignment = effAlign;
  header->RequestedSize = size;
  header->AllocLabel = label;
  header->RawOffset = reinterpret_cast<uintptr_t>(header) - rawAddr;

  return reinterpret_cast<void*>(userAddr);
}

inline u64 TotalAllocSize(u64 userSize, u32 userAlignment) noexcept
{
  const u32 effAlign = EffectiveAlignment(userAlignment);
  return sizeof(AllocHeader) + (effAlign - 1) + userSize;
}

//------------------------------------------------------------
// IAllocator Interface
//------------------------------------------------------------

struct IAllocator
{
  GECKO_API virtual ~IAllocator() = default;

  GECKO_API virtual void* Alloc(u64 size, u32 alignment) noexcept = 0;
  GECKO_API virtual void Free(void* ptr) noexcept = 0;

  // Use via LabelScope / GECKO_PUSH_LABEL to ensure proper pairing.
  GECKO_API virtual void PushLabel(Label label) noexcept = 0;
  GECKO_API virtual void PopLabel() noexcept = 0;
  GECKO_API virtual Label CurrentLabel() const noexcept = 0;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;
};

// Never returns null — defaults to SystemAllocator before services are
// installed.
GECKO_API IAllocator* GetAllocator() noexcept;

[[nodiscard]]
inline void* AllocBytes(u64 size,
                        u32 alignment = alignof(::std::max_align_t)) noexcept
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");
  return GetAllocator()->Alloc(size, alignment);
}

inline void DeallocBytes(void* ptr) noexcept
{
  if (ptr)
    GetAllocator()->Free(ptr);
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
    GetAllocator()->PushLabel(label);
  }
  ~LabelScope() noexcept
  {
    GetAllocator()->PopLabel();
  }
  LabelScope(const LabelScope&) = delete;
  LabelScope& operator=(const LabelScope&) = delete;
};

#define GECKO_PUSH_LABEL_CONCAT_(x, y) x##y
#define GECKO_PUSH_LABEL_CONCAT(x, y) GECKO_PUSH_LABEL_CONCAT_(x, y)
#define GECKO_PUSH_LABEL(label)                                             \
  ::gecko::LabelScope GECKO_PUSH_LABEL_CONCAT(_g_label_scope_, __COUNTER__) \
  {                                                                         \
    (label)                                                                 \
  }

//------------------------------------------------------------
// SystemAllocator
//------------------------------------------------------------

struct SystemAllocator final : IAllocator
{
  GECKO_API void* Alloc(u64 size, u32 alignment) noexcept override;
  GECKO_API void Free(void* ptr) noexcept override;

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
