#pragma once

#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace gecko::runtime {

class RingLogger final : public ILogger
{
public:
  explicit RingLogger(size_t capacity = 4096) noexcept;
  RingLogger() noexcept;
  virtual ~RingLogger();

  virtual void LogV(LogLevel level, Label label, const char* fmt,
                    va_list) noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  virtual void AddSinkImpl(ILogSink* sink) noexcept override;
  virtual void RemoveSinkImpl(ILogSink* sink) noexcept override;

  virtual void SetLevel(LogLevel level) noexcept override
  {
    m_Level.store(level, std::memory_order_relaxed);
  }
  virtual LogLevel Level() const noexcept override
  {
    return m_Level.load(std::memory_order_relaxed);
  }

  virtual void Flush() noexcept override;

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

  IProfiler* m_Profiler;

  void ProcessLogEntries() noexcept;
  void TryScheduleConsumerJob() noexcept;
  void ScheduleNextConsumerJob() noexcept;
  bool HasPendingEntries() const noexcept;
  static u64 NowNs() noexcept;
  static u32 ThreadId() noexcept;
};
}  // namespace gecko::runtime
