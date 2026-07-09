#pragma once

/// @file
/// `CrashSafeTraceProfilerSink` -- synchronous Chrome-trace JSON sink
/// that keeps the file in a valid state across abnormal termination.

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

/// Chrome-trace JSON sink that flushes after every event group and
/// keeps the file as valid JSON even if the process aborts. Slower
/// than `AsyncTraceProfilerSink` -- use for crash-debug builds.
class CrashSafeTraceProfilerSink final : public IProfilerSink
{
public:
  /// Open `path` for crash-safe writing.
  explicit CrashSafeTraceProfilerSink(const char* path);
  ~CrashSafeTraceProfilerSink();

  /// `true` if the underlying file was opened successfully.
  bool IsOpen() const noexcept;

  virtual void Write(const ProfEvent& event) noexcept override;
  virtual void WriteBatch(::gecko::Span<const ProfEvent> events) noexcept override;
  virtual void Flush() noexcept override;

private:
  struct Impl;
  ::gecko::Unique<Impl> m_Impl;

  void WriteEvent(const ProfEvent& event) noexcept;
  void WriteSeparator() noexcept;
  void EnsureValidJson() noexcept;
};

}  // namespace gecko::runtime
