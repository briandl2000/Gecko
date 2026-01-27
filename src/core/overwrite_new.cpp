#if GECKO_OVERRIDE_NEW
#include "gecko/core/assert.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/types.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

// ============================================================================
// Fallback Allocation Tracker
// ============================================================================
// When allocations happen before GECKO_BOOT (e.g., STL debug iterators on
// MSVC/clang-cl), we fall back to the system allocator and track them here so
// delete knows to use the system deallocator instead of the gecko allocator.
//
// Design goals:
//   1. Fast O(1) average lookup (hash set with linear probing)
//   2. Lock-free using atomic CAS operations
//   3. No dynamic allocation (fixed-size table)
//   4. Handles aligned allocations correctly on MSVC
// ============================================================================

namespace {

// We use a simple open-addressed hash set with linear probing.
// The table size should be a power of 2 and large enough for typical pre-boot
// allocations.
constexpr ::gecko::usize kFallbackTableSize = 256;
constexpr ::gecko::usize kFallbackTableMask = kFallbackTableSize - 1;

// Tombstone marker for deleted entries (must be a value that's never a valid
// pointer) Using 1 as it's never aligned properly for any allocation
void* const kTombstone =
    reinterpret_cast<void*>(static_cast<::std::uintptr_t>(1));

// Entry: stores pointer and whether it was an aligned allocation
struct FallbackEntry
{
  std::atomic<void*> ptr {nullptr};
  std::atomic<bool> isAligned {false};
};

FallbackEntry g_FallbackTable[kFallbackTableSize];
std::atomic<::gecko::usize> g_FallbackCount {0};

// Simple hash function for pointers
inline ::gecko::usize HashPtr(void* p) noexcept
{
  auto val = reinterpret_cast<::std::uintptr_t>(p);
  // Mix bits to spread sequential addresses
  val ^= val >> 16;
  val *= 0x85ebca6b;
  val ^= val >> 13;
  return static_cast<::gecko::usize>(val);
}

// Register a fallback allocation. Returns true if successfully tracked.
bool RegisterFallback(void* ptr, bool aligned) noexcept
{
  if (!ptr)
    return false;

  ::gecko::usize idx = HashPtr(ptr) & kFallbackTableMask;

  // Linear probe to find an empty or tombstone slot
  for (::gecko::usize i = 0; i < kFallbackTableSize; ++i)
  {
    ::gecko::usize slot = (idx + i) & kFallbackTableMask;
    void* stored = g_FallbackTable[slot].ptr.load(std::memory_order_acquire);

    // Try to use an empty slot
    if (stored == nullptr)
    {
      void* expected = nullptr;
      if (g_FallbackTable[slot].ptr.compare_exchange_strong(
              expected, ptr, std::memory_order_release,
              std::memory_order_relaxed))
      {
        g_FallbackTable[slot].isAligned.store(aligned,
                                              std::memory_order_release);
        g_FallbackCount.fetch_add(1, std::memory_order_relaxed);
        return true;
      }
      // CAS failed, someone else took this slot, continue probing
      continue;
    }

    // Try to use a tombstone slot
    if (stored == kTombstone)
    {
      void* expected = kTombstone;
      if (g_FallbackTable[slot].ptr.compare_exchange_strong(
              expected, ptr, std::memory_order_release,
              std::memory_order_relaxed))
      {
        g_FallbackTable[slot].isAligned.store(aligned,
                                              std::memory_order_release);
        g_FallbackCount.fetch_add(1, std::memory_order_relaxed);
        return true;
      }
      // CAS failed, someone else took this slot, continue probing
      continue;
    }

    // Slot is occupied, continue probing
  }

  // Table is full - this shouldn't happen with reasonable pre-boot allocations
  GECKO_ASSERT(false && "Fallback allocation table is full!");
  return false;
}

// Check if ptr is a fallback allocation and unregister it.
// Returns: 0 = not found, 1 = found (unaligned), 2 = found (aligned)
int UnregisterFallback(void* ptr) noexcept
{
  if (!ptr)
    return 0;

  // Fast path: if no fallbacks registered, skip lookup
  if (g_FallbackCount.load(std::memory_order_relaxed) == 0)
    return 0;

  ::gecko::usize idx = HashPtr(ptr) & kFallbackTableMask;

  // Linear probe to find the entry
  for (::gecko::usize i = 0; i < kFallbackTableSize; ++i)
  {
    ::gecko::usize slot = (idx + i) & kFallbackTableMask;
    void* stored = g_FallbackTable[slot].ptr.load(std::memory_order_acquire);

    if (stored == ptr)
    {
      // Read alignment flag before CAS (might be stale if CAS fails)
      bool aligned =
          g_FallbackTable[slot].isAligned.load(std::memory_order_acquire);

      // Use CAS to atomically replace ptr with tombstone
      void* expected = ptr;
      if (g_FallbackTable[slot].ptr.compare_exchange_strong(
              expected, kTombstone, std::memory_order_release,
              std::memory_order_relaxed))
      {
        g_FallbackTable[slot].isAligned.store(false, std::memory_order_release);
        g_FallbackCount.fetch_sub(1, std::memory_order_relaxed);
        return aligned ? 2 : 1;
      }
      // CAS failed - another thread modified this slot, re-read and continue
      continue;
    }

    // Tombstone - continue probing (slot was deleted but chain continues)
    if (stored == kTombstone)
      continue;

    // Empty slot means the entry doesn't exist (end of probe chain)
    if (stored == nullptr)
      return 0;
  }

  return 0;
}

// System allocation fallback (used before GECKO_BOOT)
void* SystemAlloc(::gecko::usize size, ::gecko::usize alignment) noexcept
{
  bool needsAligned = alignment > alignof(std::max_align_t);

  void* ptr = nullptr;
  if (needsAligned)
  {
#if defined(_MSC_VER)
    ptr = ::_aligned_malloc(size, alignment);
#else
    if (::posix_memalign(&ptr, alignment, size) != 0)
      ptr = nullptr;
#endif
  }
  else
  {
    ptr = ::std::malloc(size);
  }

  if (ptr)
  {
    RegisterFallback(ptr, needsAligned);
  }

  return ptr;
}

// System deallocation for fallback allocations
void SystemFree(void* ptr, bool aligned) noexcept
{
  if (!ptr)
    return;

#if defined(_MSC_VER)
  if (aligned)
  {
    ::_aligned_free(ptr);
  }
  else
  {
    ::std::free(ptr);
  }
#else
  // On POSIX, posix_memalign returns memory that can be freed with free()
  ::std::free(ptr);
#endif
}

}  // namespace

// ============================================================================
// Global operator new/delete overrides
// ============================================================================

void* operator new(::gecko::usize size)
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");

  auto* allocator = ::gecko::GetAllocator();

  // If gecko allocator is available, use it
  if (allocator)
  {
    if (void* ptr = allocator->Alloc(static_cast<::gecko::u64>(size),
                                     alignof(::std::max_align_t)))
    {
      return ptr;
    }
    throw ::std::bad_alloc {};
  }

  // Fallback to system allocator (before GECKO_BOOT)
  if (void* ptr = SystemAlloc(size, alignof(::std::max_align_t)))
  {
    return ptr;
  }

  throw ::std::bad_alloc {};
}

void operator delete(void* ptr) noexcept
{
  if (!ptr)
    return;

  // Check if this was a fallback allocation
  int fallbackType = UnregisterFallback(ptr);
  if (fallbackType != 0)
  {
    SystemFree(ptr, fallbackType == 2);
    return;
  }

  auto* allocator = ::gecko::GetAllocator();
  if (allocator)
  {
    allocator->Free(ptr);
    return;
  }

  // If no allocator and not a fallback, this is a leak after UninstallServices
  // We can't safely free it without knowing how it was allocated
}

void* operator new(::gecko::usize size, ::std::align_val_t align)
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");

  auto alignment = static_cast<::gecko::usize>(align);
  auto* allocator = ::gecko::GetAllocator();

  // If gecko allocator is available, use it
  if (allocator)
  {
    if (void* ptr = allocator->Alloc(static_cast<::gecko::u64>(size),
                                     static_cast<::gecko::u32>(alignment)))
    {
      return ptr;
    }
    throw ::std::bad_alloc {};
  }

  // Fallback to system allocator (before GECKO_BOOT)
  if (void* ptr = SystemAlloc(size, alignment))
  {
    return ptr;
  }

  throw ::std::bad_alloc {};
}

void operator delete(void* ptr, ::std::align_val_t) noexcept
{
  if (!ptr)
    return;

  // Check if this was a fallback allocation
  int fallbackType = UnregisterFallback(ptr);
  if (fallbackType != 0)
  {
    SystemFree(ptr, fallbackType == 2);
    return;
  }

  auto* allocator = ::gecko::GetAllocator();
  if (allocator)
  {
    allocator->Free(ptr);
  }
}

void operator delete(void* ptr, ::gecko::usize) noexcept
{
  if (!ptr)
    return;

  // Check if this was a fallback allocation
  int fallbackType = UnregisterFallback(ptr);
  if (fallbackType != 0)
  {
    SystemFree(ptr, fallbackType == 2);
    return;
  }

  auto* allocator = ::gecko::GetAllocator();
  if (allocator)
  {
    allocator->Free(ptr);
  }
}

void operator delete(void* ptr, ::gecko::usize, ::std::align_val_t) noexcept
{
  if (!ptr)
    return;

  // Check if this was a fallback allocation
  int fallbackType = UnregisterFallback(ptr);
  if (fallbackType != 0)
  {
    SystemFree(ptr, fallbackType == 2);
    return;
  }

  auto* allocator = ::gecko::GetAllocator();
  if (allocator)
  {
    allocator->Free(ptr);
  }
}

#endif
