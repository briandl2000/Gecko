#pragma once

#include "gecko/core/core.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace gecko::runtime {

struct MemCategoryStats {
  std::atomic<u64> LiveBytes{0};
  std::atomic<u64> Allocs{0};
  std::atomic<u64> Frees{0};
  Category Cat{};

  MemCategoryStats() = default;

  // Make non-copyable to avoid atomic copy issues
  MemCategoryStats(const MemCategoryStats &) = delete;
  MemCategoryStats &operator=(const MemCategoryStats &) = delete;

  // Allow move operations
  MemCategoryStats(MemCategoryStats &&other) noexcept
      : LiveBytes(other.LiveBytes.load()), Allocs(other.Allocs.load()),
        Frees(other.Frees.load()), Cat(other.Cat) {}

  MemCategoryStats &operator=(MemCategoryStats &&other) noexcept {
    if (this != &other) {
      LiveBytes.store(other.LiveBytes.load());
      Allocs.store(other.Allocs.load());
      Frees.store(other.Frees.load());
      Cat = other.Cat;
    }
    return *this;
  }
};

// Custom allocator template that bypasses tracking for internal containers
template <typename T> class UpstreamAllocator {
public:
  using value_type = T;

  explicit UpstreamAllocator(IAllocator *upstream) noexcept
      : m_Upstream(upstream) {}

  template <typename U>
  UpstreamAllocator(const UpstreamAllocator<U> &other) noexcept
      : m_Upstream(other.m_Upstream) {}

  T *allocate(std::size_t n) {
    if (!m_Upstream)
      return nullptr;
    auto *ptr = m_Upstream->Alloc(n * sizeof(T), alignof(T), {});
    return static_cast<T *>(ptr);
  }

  void deallocate(T *ptr, std::size_t n) noexcept {
    if (m_Upstream && ptr) {
      m_Upstream->Free(ptr, n * sizeof(T), alignof(T), {});
    }
  }

  template <typename U>
  bool operator==(const UpstreamAllocator<U> &other) const noexcept {
    return m_Upstream == other.m_Upstream;
  }

  template <typename U>
  bool operator!=(const UpstreamAllocator<U> &other) const noexcept {
    return !(*this == other);
  }

  template <typename U> friend class UpstreamAllocator;

private:
  IAllocator *m_Upstream;
};

class TrackingAllocator final : public IAllocator {
public:
  explicit TrackingAllocator(IAllocator *upstream) noexcept
      : m_Upstream(upstream),
        m_ByCat(UpstreamAllocator<std::pair<const u32, MemCategoryStats>>(
            upstream)) {}

  virtual void *Alloc(u64 size, u32 alignment,
                      Category category) noexcept override;
  virtual void Free(void *ptr, u64 size, u32 alignment,
                    Category category) noexcept override;

  void SetProfiler(IProfiler *profiler) noexcept { m_Profiler = profiler; }

  u64 TotalLiveBytes() const noexcept {
    return m_TotalLive.load(std::memory_order_relaxed);
  }

  bool StatsFor(Category category, MemCategoryStats &outStats) const;

  void Snapshot(std::unordered_map<u32, MemCategoryStats> &out) const;

  void EmitCounters() noexcept;

  void ResetCounters() noexcept;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

private:
  IAllocator *m_Upstream{nullptr};

  mutable std::mutex m_Mutex;

  std::unordered_map<u32, MemCategoryStats, std::hash<u32>, std::equal_to<u32>,
                     UpstreamAllocator<std::pair<const u32, MemCategoryStats>>>
      m_ByCat;

  std::atomic<u64> m_TotalLive{0};

  IProfiler *m_Profiler{nullptr};

  MemCategoryStats &EnsureCategoryLocked(Category category);
};

} // namespace gecko::runtime
