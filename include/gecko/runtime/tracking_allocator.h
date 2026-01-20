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

constexpr u32 AllocHeaderMagic = 0x47454B4F;  // "GEKO"

struct AllocHeader
{
  u32 Magic;
  u32 Alignment;
  u64 RequestedSize;
  Label AllocLabel;
  u64 RawOffset;
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

inline bool IsValidAllocHeader(const AllocHeader* header) noexcept
{
  return header && header->Magic == AllocHeaderMagic;
}

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
// Uses upstream directly without going through TrackingAllocator
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
    return static_cast<T*>(m_Upstream->Alloc(n * sizeof(T), alignof(T)));
  }

  void deallocate(T* ptr, std::size_t n) noexcept
  {
    (void)n;
    if (m_Upstream && ptr)
    {
      m_Upstream->Free(ptr);
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

constexpr u32 MaxLabelStackDepth = 64;

class TrackingAllocator final : public IAllocator
{
public:
  explicit TrackingAllocator(IAllocator* upstream) noexcept
      : m_Upstream(upstream),
        m_ByLabel(
            UpstreamAllocator<std::pair<const u64, MemLabelStats>>(upstream))
  {}

  // IAllocator interface
  void* Alloc(u64 size, u32 alignment) noexcept override;
  void Free(void* ptr) noexcept override;

  // Label stack - thread-local, used by GECKO_SCOPE macros
  void PushLabel(Label label) noexcept override;
  void PopLabel() noexcept override;
  Label CurrentLabel() const noexcept override;

  bool Init() noexcept override;
  void Shutdown() noexcept override;

  // TrackingAllocator-specific methods
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
