#pragma once

#include "gecko/core/jobs.h"
#include "gecko/core/profiler.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace gecko::runtime {

class RingProfiler final : public IProfiler
{
public:
  explicit RingProfiler(size_t capacityPow2 = 1u << 20);
  ~RingProfiler();

  void Emit(const ProfEvent& event) noexcept override;
  u64 NowNs() const noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  bool TryPop(ProfEvent& event) noexcept;

  // Sink management
  void AddSink(IProfilerSink* sink) noexcept;
  void RemoveSink(IProfilerSink* sink) noexcept;

private:
  struct Slot
  {
    std::atomic<u64> Sequence {0};
    ProfEvent ProfileEvent {};
  };
  std::vector<Slot> m_Ring {};
  size_t m_Mask {0};
  std::atomic<u64> m_Head {0};
  std::atomic<u64> m_Tail {0};

  // Sink storage with thread safety
  std::vector<IProfilerSink*> m_Sinks {};
  std::mutex m_SinkMu {};

  // Async consumer system
  std::atomic<bool> m_Run {true};
  JobHandle m_ConsumerJob {};
  Label m_ProfilerLabel {};

  void ProcessProfEvents() noexcept;
  void TryScheduleConsumerJob() noexcept;
  void ScheduleNextConsumerJob() noexcept;
  bool HasPendingEvents() const noexcept;

  static u64 MonotonicNowNs() noexcept;
};

u32 ThisThreadId() noexcept;

}  // namespace gecko::runtime
