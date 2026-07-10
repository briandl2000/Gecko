#pragma once

/// @file
/// `RingLogger` -- lock-free MPSC ring-buffer `ILogger` implementation.
///
/// Writers push fixed-size entries into a power-of-two ring; a
/// background consumer job drains them and forwards to sinks. Suitable
/// for production / runtime use where log calls must not stall the
/// hot path.

#include "gecko/core/ptr.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

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

  virtual void SetLevel(LogLevel level) noexcept override;
  virtual LogLevel Level() const noexcept override;

  virtual void Flush() noexcept override;

  /// Optional profiler used to instrument the consumer-drain job.
  void SetProfiler(IProfiler* profiler) noexcept;

private:
  struct Impl;
  ::gecko::Unique<Impl> m_Impl;

  void ProcessLogEntries() noexcept;
  void TryScheduleConsumerJob() noexcept;
  void ScheduleNextConsumerJob() noexcept;
  bool HasPendingEntries() const noexcept;
  static u64 NowNs() noexcept;
  static u32 ThreadId() noexcept;
};
}  // namespace gecko::runtime
