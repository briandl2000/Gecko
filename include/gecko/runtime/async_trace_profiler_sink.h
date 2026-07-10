#pragma once

/// @file
/// `AsyncTraceProfilerSink` -- buffered Chrome-trace JSON sink with a
/// dedicated worker thread.

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

/// Asynchronous Chrome-trace JSON sink. Pushes events into an internal
/// double-buffered queue; a dedicated worker thread drains, formats
/// and writes the JSON, fsync-ing every ~100 ms. Drains in the
/// destructor.
///
/// Use this for profiling sessions where throughput matters and a
/// clean shutdown is guaranteed. For shipping / crash-debug builds
/// prefer `CrashSafeTraceProfilerSink`, which preserves a valid JSON
/// document on abnormal exit at the cost of synchronous writes.
class AsyncTraceProfilerSink final : public IProfilerSink
{
public:
  /// Open `path` for writing. The file is left empty on failure.
  explicit AsyncTraceProfilerSink(const char* path);
  ~AsyncTraceProfilerSink();

  AsyncTraceProfilerSink(const AsyncTraceProfilerSink&) = delete;
  AsyncTraceProfilerSink& operator=(const AsyncTraceProfilerSink&) = delete;

  /// `true` if the underlying file was opened successfully.
  bool IsOpen() const noexcept;

  /// Drop any event whose level is more verbose than `level` before
  /// queueing for the worker. Stats / aggregates inside `IProfiler`
  /// are unaffected -- this only thins the Chrome-trace JSON.
  /// Default: `Detailed` (no filtering).
  void SetMinLevel(ProfLevel level) noexcept;

  /// Current minimum-level filter.
  ProfLevel GetMinLevel() const noexcept;

  void Write(const ProfEvent& event) noexcept override;
  void WriteBatch(::gecko::Span<const ProfEvent> events) noexcept override;
  void Flush() noexcept override;

private:
  struct Impl;
  ::gecko::Unique<Impl> m_Impl;

  void WorkerLoop() noexcept;
};

}  // namespace gecko::runtime
