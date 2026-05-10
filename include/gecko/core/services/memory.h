#pragma once

/// @file
/// Allocator interface, label stack, and platform/system allocators.
///
/// The allocator is *infrastructure*, not a service: it is always
/// available from the first instruction of `main()` onwards, before
/// any module has booted. Heap allocations from the engine and
/// applications go through `IAllocator`; install a custom one with
/// `SetAllocator()` to enable tracking, pools, etc.

#include "gecko/core/api.h"
#include "gecko/core/assert.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

#include <cstdint>

namespace gecko {

struct LabelScope;

// ------------------------------------------------------------
// Allocation Header
// ------------------------------------------------------------

// All `IAllocator` implementations prepend an `AllocHeader` to every
// user pointer. The `Magic` field enables safe cross-allocator frees
// (e.g. allocations made before the engine booted being freed after
// shutdown).

/// Magic value `"SYSA"` written into headers from `SystemAllocator`.
constexpr u32 SystemAllocMagic = 0x53595341;
/// Magic value `"GEKO"` written into headers from `TrackingAllocator`.
constexpr u32 TrackingAllocMagic = 0x47454B4F;

/// Per-allocation header placed immediately before the user pointer.
struct AllocHeader
{
  u32 Magic;          ///< One of `*AllocMagic` constants.
  u32 Alignment;      ///< Effective alignment used for the allocation.
  u64 RequestedSize;  ///< Size requested by the caller.
  Label AllocLabel;   ///< Label active when the allocation was made.
  u64 RawOffset;      ///< Bytes from this header back to the raw platform ptr.
};

/// Recover the header for a user pointer returned by `IAllocator::Alloc`.
inline AllocHeader* HeaderFromUserPtr(void* userPtr) noexcept
{
  return reinterpret_cast<AllocHeader*>(static_cast<u8*>(userPtr) - sizeof(AllocHeader));
}

/// Recover the raw platform pointer that needs to be passed to `PlatformFree`.
inline void* RawPtrFromHeader(AllocHeader* header) noexcept
{
  return reinterpret_cast<u8*>(header) - header->RawOffset;
}

/// Test whether `header` was produced by a known Gecko allocator.
/// @param header Candidate header pointer; may be null.
/// @return `true` if the header carries a recognised allocator magic.
inline bool IsAllocHeaderValid(const AllocHeader* header) noexcept
{
  return header && (header->Magic == SystemAllocMagic || header->Magic == TrackingAllocMagic);
}

// ------------------------------------------------------------
// Platform Allocation
// ------------------------------------------------------------

/// Aligned allocation directly from the OS (bypasses `IAllocator`).
/// @param size Number of bytes to allocate.
/// @param alignment Required alignment (power of two).
GECKO_API void* PlatformAlloc(u64 size, u32 alignment) noexcept;
/// Pair to `PlatformAlloc`.
/// @param ptr Pointer previously returned by `PlatformAlloc`. May be null.
/// @param alignment Must match the original `PlatformAlloc` request.
GECKO_API void PlatformFree(void* ptr, u32 alignment) noexcept;

// ------------------------------------------------------------
// Allocation Header Helpers
// ------------------------------------------------------------

/// Returns the user-alignment rounded up to at least `alignof(AllocHeader)`.
inline u32 EffectiveAlignment(u32 userAlignment) noexcept
{
  return userAlignment > alignof(AllocHeader) ? userAlignment : static_cast<u32>(alignof(AllocHeader));
}

/// Carve a header out of a raw platform allocation and return the
/// aligned user pointer.
///
/// @param rawPtr Pointer obtained from
///        `PlatformAlloc(TotalAllocSize(size, userAlignment),
///        EffectiveAlignment(userAlignment))`.
/// @param size Original user-requested byte count.
/// @param userAlignment User-requested alignment (power of two).
/// @param magic Allocator magic to record in the header.
/// @param label Label active at the call site.
/// @return Aligned user pointer immediately after the placed header.
inline void* PlaceAllocHeader(void* rawPtr, u64 size, u32 userAlignment, u32 magic, Label label = {}) noexcept
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

// ------------------------------------------------------------
// IAllocator Interface
// ------------------------------------------------------------

/// Engine-wide allocator interface.
///
/// Implementations layer custom behavior (tracking, pools, debug
/// guards, ...) on top of `PlatformAlloc`. All implementations must
/// place an `AllocHeader` immediately before the returned user
/// pointer so cross-allocator frees can route through the right
/// destructor.
struct IAllocator
{
  GECKO_API virtual ~IAllocator() = default;

  /// Allocate aligned bytes from this allocator.
  /// @param size Number of bytes to allocate.
  /// @param alignment Required alignment (power of two).
  GECKO_API virtual void* Alloc(u64 size, u32 alignment) noexcept = 0;
  /// Free a pointer previously returned by `Alloc`.
  /// @param ptr Pointer to free. Null is a no-op.
  GECKO_API virtual void Free(void* ptr) noexcept = 0;

  /// Push a label onto the per-allocator label stack. Subsequent
  /// allocations will be attributed to it. Use `LabelScope` /
  /// `GECKO_PUSH_LABEL` to guarantee correct pairing.
  /// @param label Label to push.
  GECKO_API virtual void PushLabel(Label label) noexcept = 0;
  /// Pop the most recently pushed label. Unbalanced pops are a
  /// contract violation.
  GECKO_API virtual void PopLabel() noexcept = 0;
  /// @return The label currently on top of the stack, or an empty
  ///         label when the stack is empty.
  GECKO_API virtual Label CurrentLabel() const noexcept = 0;

  /// One-time setup; called when the allocator is installed.
  GECKO_API virtual bool Init() noexcept = 0;
  /// Counterpart to `Init`; called before the allocator is replaced.
  GECKO_API virtual void Shutdown() noexcept = 0;
};

// The allocator is infrastructure, not a service: it is always
// available from the first instruction of `main()`. The default is
// a process-lifetime `SystemAllocator`.

/// @return Reference to the active allocator. Never null.
GECKO_API IAllocator& Allocator() noexcept;

/// Install a custom allocator.
///
/// Calls `Init()` on the supplied allocator. If a previous custom
/// allocator was installed it is shut down first. Pass `nullptr` to
/// revert to the default (equivalent to `ResetAllocator()`).
///
/// @param allocator Allocator to install, or null to reset.
/// @return `false` if `allocator->Init()` returned `false`.
GECKO_API bool SetAllocator(IAllocator* allocator) noexcept;

/// Shut down the currently installed custom allocator (if any) and
/// revert to the default `SystemAllocator`.
GECKO_API void ResetAllocator() noexcept;

/// RAII wrapper around `SetAllocator` / `ResetAllocator`.
///
/// Construct with a reference to a stack-owned allocator; on
/// destruction the default `SystemAllocator` is restored. Convert to
/// `bool` (or check `Ok()`) to verify `Init()` succeeded.
///
/// Typical use in `main()` or an application class:
///
/// ```cpp
/// runtime::TrackingAllocator tracker;
/// AllocatorScope             scope {tracker};
/// if (!scope) return 1;       // tracker->Init() failed
/// // ... boot engine ...
/// // scope's dtor calls ResetAllocator() last.
/// ```
class AllocatorScope
{
public:
  /// Calls `SetAllocator(&allocator)`. Records whether init succeeded.
  explicit AllocatorScope(IAllocator& allocator) noexcept : m_Ok(SetAllocator(&allocator))
  {}
  /// Calls `ResetAllocator()` if construction succeeded.
  ~AllocatorScope() noexcept
  {
    if (m_Ok)
      ResetAllocator();
  }

  AllocatorScope(const AllocatorScope&) = delete;
  AllocatorScope& operator=(const AllocatorScope&) = delete;
  AllocatorScope(AllocatorScope&&) = delete;
  AllocatorScope& operator=(AllocatorScope&&) = delete;

  /// @return `true` if the allocator's `Init()` succeeded.
  [[nodiscard]] bool Ok() const noexcept
  {
    return m_Ok;
  }
  /// @return `true` if the allocator's `Init()` succeeded.
  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_Ok;
  }

private:
  bool m_Ok;
};

/// Allocate bytes from the active allocator.
/// Asserts on zero size and non-power-of-two alignment in debug.
/// @param size Number of bytes to allocate.
/// @param alignment Required alignment (power of two).
/// @return Pointer to the new allocation; never null.
[[nodiscard]]
inline void* AllocBytes(u64 size,
                        u32 alignment = alignof(::std::max_align_t)) noexcept  // abi-ok: alignof() in default
                                                                               // arg, evaluated at the call
                                                                               // site as a u32 literal
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");
  return Allocator().Alloc(size, alignment);
}

/// Free a pointer obtained from `AllocBytes`. Null is a no-op.
inline void DeallocBytes(void* ptr) noexcept
{
  if (ptr)
    Allocator().Free(ptr);
}

/// Typed array allocator.
/// Asserts on zero count and on `count * sizeof(T)` overflow in debug.
/// @tparam T Element type.
/// @param count Number of elements (must be > 0).
/// @param alignment Element alignment; defaults to `alignof(T)`.
/// @return Pointer to the array; never null.
template <class T>
[[nodiscard]]
inline T* AllocArray(u64 count, u32 alignment = alignof(T)) noexcept
{
  GECKO_ASSERT(count > 0 && "Cannot allocate zero elements");
  GECKO_ASSERT(count <= (SIZE_MAX / sizeof(T)) && "Count would overflow");
  return static_cast<T*>(AllocBytes(sizeof(T) * count, alignment));
}

/// RAII helper that pushes a label on construction and pops it on
/// destruction. Prefer the `GECKO_PUSH_LABEL` macro over instantiating
/// this directly.
struct LabelScope
{
  LabelScope(Label label) noexcept
  {
    Allocator().PushLabel(label);
  }
  ~LabelScope() noexcept
  {
    Allocator().PopLabel();
  }
  LabelScope(const LabelScope&) = delete;
  LabelScope& operator=(const LabelScope&) = delete;
};

#define GECKO_PUSH_LABEL_CONCAT_(x, y) x##y
#define GECKO_PUSH_LABEL_CONCAT(x, y) GECKO_PUSH_LABEL_CONCAT_(x, y)
#define GECKO_PUSH_LABEL(label)                                          \
  ::gecko::LabelScope GECKO_PUSH_LABEL_CONCAT(_g_label_scope_, __LINE__) \
  {                                                                      \
    (label)                                                              \
  }

// ------------------------------------------------------------
// SystemAllocator
// ------------------------------------------------------------

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
