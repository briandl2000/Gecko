#pragma once

/// @file
/// `TrackingAllocator` -- per-`Label` tracking `IAllocator`, plus
/// supporting types (`MemLabelStats`, `MallocAllocator`).

#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>

namespace gecko::runtime {

/// Per-label live-bytes / alloc / free counters.
struct MemLabelStats
{
  std::atomic<u64> LiveBytes {0};
  std::atomic<u64> Allocs {0};
  std::atomic<u64> Frees {0};
  Label StatsLabel {};

  MemLabelStats() = default;

  MemLabelStats(const MemLabelStats&) = delete;
  MemLabelStats& operator=(const MemLabelStats&) = delete;

  MemLabelStats(MemLabelStats&& other) noexcept
      : LiveBytes(other.LiveBytes.load()), Allocs(other.Allocs.load()), Frees(other.Frees.load()),
        StatsLabel(other.StatsLabel)
  {}

  MemLabelStats& operator=(MemLabelStats&& other) noexcept
  {
    if (this != &other)
    {
      LiveBytes.store(other.LiveBytes.load());
      Allocs.store(other.Allocs.load());
      Frees.store(other.Frees.load());
      StatsLabel = other.StatsLabel;
    }
    return *this;
  }
};

/// Minimal STL allocator that bypasses `IAllocator` and goes straight
/// to `std::malloc`/`std::free`. Used inside `TrackingAllocator`'s own
/// containers so they cannot recurse back through the allocator they
/// are tracking.
template <typename T>
class MallocAllocator
{
  static_assert(alignof(T) <= alignof(::std::max_align_t), "MallocAllocator does not support over-aligned types");

public:
  using value_type = T;

  MallocAllocator() noexcept = default;

  template <typename U>
  MallocAllocator(const MallocAllocator<U>&) noexcept
  {}

  T* allocate(::std::size_t n)
  {
    void* ptr = ::std::malloc(n * sizeof(T));
    if (!ptr)
      throw ::std::bad_alloc {};
    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, ::std::size_t) noexcept
  {
    ::std::free(ptr);
  }

  template <typename U>
  bool operator==(const MallocAllocator<U>&) const noexcept
  {
    return true;
  }

  template <typename U>
  bool operator!=(const MallocAllocator<U>&) const noexcept
  {
    return false;
  }
};

/// Maximum nesting depth for `PushLabel` / `PopLabel`.
constexpr u32 MaxLabelStackDepth = 64;

/// Per-label-tracking `IAllocator`. Uses `PlatformAlloc`/`PlatformFree`
/// directly with a `TrackingAllocMagic` header. Cross-allocator frees
/// are handled: `SystemAllocMagic` allocations (made before boot) are
/// forwarded to `PlatformFree`.
class TrackingAllocator final : public IAllocator
{
public:
  TrackingAllocator() noexcept = default;

  void* Alloc(u64 size, u32 alignment) noexcept override;
  void Free(void* ptr) noexcept override;

  void PushLabel(Label label) noexcept override;
  void PopLabel() noexcept override;
  Label CurrentLabel() const noexcept override;

  bool Init() noexcept override;
  void Shutdown() noexcept override;

  /// Optional profiler used to emit per-label memory counters.
  void SetProfiler(IProfiler* profiler) noexcept
  {
    m_Profiler = profiler;
  }

  /// Total bytes currently live across all labels.
  u64 TotalLiveBytes() const noexcept
  {
    return m_TotalLive.load(std::memory_order_relaxed);
  }

  /// Copy out the stats for `label`. Returns `false` if `label` has no
  /// recorded allocations.
  bool StatsFor(Label label, MemLabelStats& outStats) const;
  /// Snapshot every label's stats into `out` (key is `Label::Id`).
  void Snapshot(std::unordered_map<u64, MemLabelStats>& out) const;
  /// Emit the current per-label counters as profiler events.
  void EmitCounters() noexcept;
  /// Reset all per-label counters back to zero (does not free memory).
  void ResetCounters() noexcept;

private:
  mutable std::mutex m_Mutex;

  std::unordered_map<u64, MemLabelStats, std::hash<u64>, std::equal_to<u64>,
                     MallocAllocator<std::pair<const u64, MemLabelStats>>>
      m_ByLabel;

  std::atomic<u64> m_TotalLive {0};

  IProfiler* m_Profiler {nullptr};

  MemLabelStats& EnsureLabelLocked(Label label);
};

}  // namespace gecko::runtime
