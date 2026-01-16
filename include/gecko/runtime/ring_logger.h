#pragma once

#include "gecko/core/jobs.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"
#include "gecko/core/types.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace gecko::runtime {

class RingLogger final : public ILogger
{
public:
  explicit RingLogger(size_t capacity = 4096);
  virtual ~RingLogger();

  virtual void LogV(LogLevel level, Label label, const char* fmt,
                    va_list) noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  virtual void AddSink(ILogSink* sink) noexcept override;
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
    std::atomic<u64> Sequence;
    LogLevel Level;
    Label EntryLabel;
    u64 TimeNs;
    u32 ThreadId;
    char Text[512];
  };

  std::vector<Entry> m_Ring;
  size_t m_Mask {0};
  std::atomic<u64> m_Head {0};
  std::atomic<u64> m_Tail {0};

  std::mutex m_SinkMu;
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
