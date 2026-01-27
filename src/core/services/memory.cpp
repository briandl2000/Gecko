#include "gecko/core/services/memory.h"

#include "gecko/core/types.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace gecko {

// ============================================================================
// MSVC-only aligned allocation tracking
// On MSVC, _aligned_malloc must be paired with _aligned_free, not free().
// We track which allocations used aligned_malloc via a hash table.
// On POSIX systems, posix_memalign can be freed with free(), so no tracking.
// ============================================================================
#if defined(_MSC_VER)

namespace {

// Tombstone marker for deleted entries - must never be a valid pointer
// Using 1 as it's never properly aligned for any allocation
void* const kTombstone =
    reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));

// Table size should be power of 2, large enough for typical aligned allocs
constexpr usize kAlignedTableSize = 256;
constexpr usize kAlignedTableMask = kAlignedTableSize - 1;

std::atomic<void*> g_AlignedTable[kAlignedTableSize];
std::atomic<usize> g_AlignedCount {0};

// Simple hash function for pointers
inline usize HashPtr(void* p) noexcept
{
  auto val = reinterpret_cast<std::uintptr_t>(p);
  val ^= val >> 16;
  val *= 0x85ebca6b;
  val ^= val >> 13;
  return static_cast<usize>(val);
}

// Register an aligned allocation
void RegisterAligned(void* ptr) noexcept
{
  if (!ptr)
    return;

  usize idx = HashPtr(ptr) & kAlignedTableMask;

  for (usize i = 0; i < kAlignedTableSize; ++i)
  {
    usize slot = (idx + i) & kAlignedTableMask;
    void* stored = g_AlignedTable[slot].load(std::memory_order_acquire);

    // Try to use an empty slot
    if (stored == nullptr)
    {
      void* expected = nullptr;
      if (g_AlignedTable[slot].compare_exchange_strong(
              expected, ptr, std::memory_order_release,
              std::memory_order_relaxed))
      {
        g_AlignedCount.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      // CAS failed, continue probing
      continue;
    }

    // Try to use a tombstone slot
    if (stored == kTombstone)
    {
      void* expected = kTombstone;
      if (g_AlignedTable[slot].compare_exchange_strong(
              expected, ptr, std::memory_order_release,
              std::memory_order_relaxed))
      {
        g_AlignedCount.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      // CAS failed, continue probing
      continue;
    }

    // Slot is occupied, continue probing
  }

  // Table full - shouldn't happen with reasonable usage
  GECKO_ASSERT(false && "Aligned allocation table is full!");
}

// Check if ptr is an aligned allocation and unregister it
bool UnregisterAligned(void* ptr) noexcept
{
  if (!ptr)
    return false;

  if (g_AlignedCount.load(std::memory_order_relaxed) == 0)
    return false;

  usize idx = HashPtr(ptr) & kAlignedTableMask;

  for (usize i = 0; i < kAlignedTableSize; ++i)
  {
    usize slot = (idx + i) & kAlignedTableMask;
    void* stored = g_AlignedTable[slot].load(std::memory_order_acquire);

    if (stored == ptr)
    {
      // Use CAS to atomically replace ptr with tombstone
      void* expected = ptr;
      if (g_AlignedTable[slot].compare_exchange_strong(
              expected, kTombstone, std::memory_order_release,
              std::memory_order_relaxed))
      {
        g_AlignedCount.fetch_sub(1, std::memory_order_relaxed);
        return true;
      }
      // CAS failed - another thread modified this slot, re-read and continue
      continue;
    }

    // Tombstone - continue probing (slot was deleted but chain continues)
    if (stored == kTombstone)
      continue;

    // Empty slot means entry doesn't exist (end of probe chain)
    if (stored == nullptr)
      return false;
  }

  return false;
}

}  // namespace

#endif  // defined(_MSC_VER)

void* SystemAllocator::Alloc(u64 size, u32 alignment) noexcept
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  // For small alignments, just use malloc - can be freed with free()
  if (alignment <= alignof(std::max_align_t))
  {
    return ::std::malloc(static_cast<usize>(size));
  }

// For larger alignments, use platform-specific aligned allocation
#if defined(_MSC_VER)
  void* ptr = ::_aligned_malloc(static_cast<usize>(size), alignment);
  if (ptr)
  {
    RegisterAligned(ptr);
  }
  return ptr;
#else
  void* ptr = nullptr;
  if (::posix_memalign(&ptr, alignment, static_cast<usize>(size)) != 0)
    return nullptr;
  return ptr;
#endif
}

void SystemAllocator::Free(void* ptr) noexcept
{
  if (!ptr)
    return;

#if defined(_MSC_VER)
  if (UnregisterAligned(ptr))
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

bool SystemAllocator::Init() noexcept
{
  return true;
}

void SystemAllocator::Shutdown() noexcept
{}

}  // namespace gecko
