#pragma once

/// @file
/// `TraceFileSink` -- buffered Chrome-trace JSON sink (single-thread).

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

/// Mutex-guarded Chrome-trace JSON sink. Buffers events in memory and
/// flushes on `Flush()` or destruction. Simpler than the async sink;
/// use when you don't need a worker thread.
class TraceFileSink final : public IProfilerSink
{
public:
  /// Open `path` for writing.
  explicit TraceFileSink(const char* path);
  ~TraceFileSink();

  /// `true` if the underlying file was opened successfully.
  bool IsOpen() const noexcept;

  virtual void Write(const ProfEvent& event) noexcept override;
  virtual void WriteBatch(::gecko::Span<const ProfEvent> events) noexcept override;
  virtual void Flush() noexcept override;

private:
  struct Impl;
  ::gecko::Unique<Impl> m_Impl;

  void WriteJsonEvent(const ProfEvent& event) noexcept;
  void FlushBufferedEvents() noexcept;
};

}  // namespace gecko::runtime
