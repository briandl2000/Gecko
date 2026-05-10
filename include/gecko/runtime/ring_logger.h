#pragma once

/// @file
/// `RingLogger` -- lock-free MPSC ring-buffer `ILogger` implementation.
///
/// Writers push fixed-size entries into a power-of-two ring; a
/// background consumer job drains them and forwards to sinks. Suitable
/// for production / runtime use where log calls must not stall the
/// hot path.

#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace gecko::runtime {

/// Asynchronous logger backed by a fixed-capacity ring buffer.
/// Drained by a job-system task that batches writes to sinks.
class RingLogger final : public ILogger
{
public:
  /// Construct with `capacity` log slots (rounded up to a power of two).
  explicit RingLogger(size_t capacity = 4096) noexcept;
  RingLogger() noexcept;
  virtual ~RingLogger();

  virtual void LogV(LogLevel level, Label label, const char* fmt, va_list) noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  virtual void AddSink(ILogSink* sink) noexcept override;
  virtual void RemoveSink(ILogSink* sink) noexcept override;

  virtual void SetLevel(LogLevel level) noexcept override
  {
    m_Level.store(level, std::memory_order_relaxed);
  }
  virtual LogLevel Level() const noexcept override
  {
    return m_Level.load(std::memory_order_relaxed);
  }

  virtual void Flush() noexcept override;

  /// Optional profiler used to instrument the consumer-drain job.
  void SetProfiler(IProfiler* profiler) noexcept
  {
    m_Profiler = profiler;
  }

private:
  struct Entry
  {
    std::atomic<u64> Sequence {0};
    LogLevel Level {LogLevel::Info};
    Label EntryLabel {};
    u64 TimeNs {0};
    u32 ThreadId {0};
    char Text[512] {};

    Entry() = default;

    // Atomics are not copyable or moveable, so we delete these
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
  };

  std::vector<Entry> m_Ring;
  size_t m_Capacity {4096};
  size_t m_Mask {0};
  std::atomic<u64> m_Head {0};
  std::atomic<u64> m_Tail {0};

  std::mutex m_SinkMu;
  std::mutex m_JobMu;  // Protects m_ConsumerJob
  std::vector<ILogSink*> m_Sinks;

  std::atomic<LogLevel> m_Level {LogLevel::Info};
  std::atomic<bool> m_Run {true};
  std::atomic<u64> m_Dropped {0};

  JobHandle m_ConsumerJob;
  Label m_LoggerLabel;

  // Per-instance rate-limit timestamp for consumer-job scheduling. See
  // matching comment in ring_profiler.h for why this is not a function-
  // local static.
  std::atomic<u64> m_LastScheduleNs {0};

  IProfiler* m_Profiler;

  void ProcessLogEntries() noexcept;
  void TryScheduleConsumerJob() noexcept;
  void ScheduleNextConsumerJob() noexcept;
  bool HasPendingEntries() const noexcept;
  static u64 NowNs() noexcept;
  static u32 ThreadId() noexcept;
};
}  // namespace gecko::runtime
