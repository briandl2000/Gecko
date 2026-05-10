#pragma once

/// @file
/// `TraceFileSink` -- buffered Chrome-trace JSON sink (single-thread).

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/platform_io.h"

#include <mutex>
#include <vector>

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
  bool IsOpen() const noexcept
  {
    return m_Writer != nullptr;
  }

  virtual void Write(const ProfEvent& event) noexcept override;
  virtual void WriteBatch(::gecko::Span<const ProfEvent> events) noexcept override;
  virtual void Flush() noexcept override;

private:
  ::gecko::Unique<::gecko::platform::FileWriter> m_Writer {};
  bool m_First {true};
  u64 m_Time0Ns {0};
  std::vector<ProfEvent> m_BufferedEvents {};
  std::mutex m_Mutex {};

  void WriteJsonEvent(const ProfEvent& event) noexcept;
  void FlushBufferedEvents() noexcept;
};

}  // namespace gecko::runtime
