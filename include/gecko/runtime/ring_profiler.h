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

  void ProcessProfEvents() noexcept;
  void TryScheduleConsumerJob() noexcept;
  void ScheduleNextConsumerJob() noexcept;
  bool HasPendingEvents() const noexcept;

  static u64 MonotonicNowNs() noexcept;
};

u32 ThisThreadId() noexcept;

}  // namespace gecko::runtime
