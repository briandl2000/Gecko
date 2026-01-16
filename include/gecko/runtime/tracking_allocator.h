#pragma once

#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace gecko::runtime {

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

// Custom allocator template that bypasses tracking for internal containers
template <typename T>
class UpstreamAllocator
{
public:
  using value_type = T;

  explicit UpstreamAllocator(IAllocator* upstream) noexcept
      : m_Upstream(upstream)
  {}

  template <typename U>
  UpstreamAllocator(const UpstreamAllocator<U>& other) noexcept
      : m_Upstream(other.m_Upstream)
  {}

  T* allocate(std::size_t n)
  {
    if (!m_Upstream)
      return nullptr;
    auto* ptr = m_Upstream->Alloc(n * sizeof(T), alignof(T), {});
    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, std::size_t n) noexcept
  {
    if (m_Upstream && ptr)
    {
      m_Upstream->Free(ptr, n * sizeof(T), alignof(T), {});
    }
  }

  template <typename U>
  bool operator==(const UpstreamAllocator<U>& other) const noexcept
  {
    return m_Upstream == other.m_Upstream;
  }

  template <typename U>
  bool operator!=(const UpstreamAllocator<U>& other) const noexcept
  {
    return !(*this == other);
  }

  template <typename U>
  friend class UpstreamAllocator;

private:
  IAllocator* m_Upstream;
};

class TrackingAllocator final : public IAllocator
{
public:
  explicit TrackingAllocator(IAllocator* upstream) noexcept
      : m_Upstream(upstream),
        m_ByLabel(
            UpstreamAllocator<std::pair<const u64, MemLabelStats>>(upstream))
  {}

  virtual void* Alloc(u64 size, u32 alignment, Label label) noexcept override;
  virtual void Free(void* ptr, u64 size, u32 alignment,
                    Label label) noexcept override;

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

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

private:
  IAllocator* m_Upstream {nullptr};

  mutable std::mutex m_Mutex;

  std::unordered_map<u64, MemLabelStats, std::hash<u64>, std::equal_to<u64>,
                     UpstreamAllocator<std::pair<const u64, MemLabelStats>>>
      m_ByLabel;

  std::atomic<u64> m_TotalLive {0};

  IProfiler* m_Profiler {nullptr};

  MemLabelStats& EnsureLabelLocked(Label label);
};

}  // namespace gecko::runtime
