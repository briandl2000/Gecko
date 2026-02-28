#pragma once

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

//------------------------------------------------------------
// Per-Label Stats
//------------------------------------------------------------

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
      : LiveBytes(other.LiveBytes.load()), Allocs(other.Allocs.load()),
        Frees(other.Frees.load()), StatsLabel(other.StatsLabel)
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

//------------------------------------------------------------
// MallocAllocator — STL allocator bypassing Gecko
//------------------------------------------------------------
// Avoids circular allocation in TrackingAllocator's internal containers.

template <typename T>
class MallocAllocator
{
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

//------------------------------------------------------------
// TrackingAllocator
//------------------------------------------------------------
// Per-label memory tracking. Uses PlatformAlloc/Free directly with
// TrackingAllocMagic. Cross-allocator frees are handled: SystemAllocMagic
// allocations (pre-boot) are forwarded to PlatformFree.

constexpr u32 MaxLabelStackDepth = 64;

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

  void SetProfiler(IProfiler* profiler) noexcept
  {
    m_Profiler = profiler;
  }

  u64 TotalLiveBytes() const noexcept
  {
    return m_TotalLive.load(std::memory_order_relaxed);
  }

  bool StatsFor(Label label, MemLabelStats& outStats) const;
  void Snapshot(std::unordered_map<u64, MemLabelStats>& out) const;
  void EmitCounters() noexcept;
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
