#pragma once

#include "gecko/core/services/jobs.h"
#include "gecko/core/services/profiler.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace gecko::runtime {

class RingProfiler final : public IProfiler
{
public:
  explicit RingProfiler(size_t capacityPow2 = 1u << 20) noexcept;
  RingProfiler() noexcept;
  ~RingProfiler();

  void Emit(const ProfEvent& event) noexcept override;
  u64 NowNs() const noexcept override;

  void SetMinLevel(ProfLevel level) noexcept override;
  ProfLevel GetMinLevel() const noexcept override;
  bool IsLevelEnabled(ProfLevel level) const noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  bool TryPop(ProfEvent& event) noexcept;

  void AddSink(IProfilerSink* sink) noexcept override;
  void RemoveSink(IProfilerSink* sink) noexcept override;

  ScopeStats GetStats(u32 nameHash) const noexcept override;
  u8 RegisterCategory(const char* name) noexcept override;
  void SetCategoryEnabled(u8 id, bool on) noexcept override;
  bool IsCategoryEnabled(u8 id) const noexcept override;
  ProfilerDiagnostics GetDiagnostics() const noexcept override;

  // Flushes all pending events to sinks
  void Flush() noexcept;

private:
  struct Slot
  {
    std::atomic<u64> Sequence {0};
    ProfEvent ProfileEvent {};

    Slot() = default;

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;
    Slot(Slot&&) = delete;
    Slot& operator=(Slot&&) = delete;
  };

  std::vector<Slot> m_Ring {};
  size_t m_Capacity {1u << 20};
  size_t m_Mask {0};
  std::atomic<u64> m_Head {0};
  std::atomic<u64> m_Tail {0};

  std::vector<IProfilerSink*> m_Sinks {};
  std::mutex m_SinkMu {};

  // Async consumer system
  std::atomic<bool> m_Run {true};
  std::mutex m_JobMu {};  // Protects m_ConsumerJob
  JobHandle m_ConsumerJob {};
  Label m_ProfilerLabel {};
  std::atomic<ProfLevel> m_MinLevel {ProfLevel::Detailed};
  std::atomic<u64> m_DroppedEvents {0};
  std::atomic<u64> m_ReentrantDrops {0};
  std::atomic<u64> m_AggregatorOverflow {0};

  // Aggregator: 1024-entry open-addressing table keyed by name-hash.
  // Updated on ZoneEnd for Always-level scopes only; reset on FrameMark.
  // Reads via GetStats() are relaxed snapshots (not transactional).
  static constexpr size_t c_AggregatorCapacity = 1024;
  struct AggSlot
  {
    std::atomic<u32> NameHash {0};
    std::atomic<u64> LastNs {0};
    std::atomic<u64> MinNs {~u64 {0}};
    std::atomic<u64> MaxNs {0};
    std::atomic<u32> Count {0};
    // Tracks the matching ZoneBegin TimestampNs on this slot's most recent
    // open zone; used by ZoneEnd to compute duration.
    std::atomic<u64> OpenBeginNs {0};
  };
  std::vector<AggSlot> m_Aggregator {};

  // Category state: 64 named slots, each with a name and an enabled bit
  // packed into m_CategoryMask.
  static constexpr u8 c_CategoryCapacity = c_ProfMaxCategories;
  std::atomic<u64> m_CategoryMask {~u64 {0}};  // all enabled by default
  std::mutex m_CategoryMu {};
  std::vector<const char*> m_CategoryNames {};

  void ProcessProfEvents() noexcept;
  void TryScheduleConsumerJob() noexcept;
  void ScheduleNextConsumerJob() noexcept;
  bool HasPendingEvents() const noexcept;

  void UpdateAggregator(const ProfEvent& ev) noexcept;
  void ResetAggregator() noexcept;

  static u64 MonotonicNowNs() noexcept;
};

u32 ThisThreadId() noexcept;

}  // namespace gecko::runtime
