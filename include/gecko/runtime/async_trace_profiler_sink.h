#pragma once

/// @file
/// `AsyncTraceProfilerSink` -- buffered Chrome-trace JSON sink with a
/// dedicated worker thread.

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/platform_io.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

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
  bool IsOpen() const noexcept
  {
    return m_Writer != nullptr;
  }

  /// Drop any event whose level is more verbose than `level` before
  /// queueing for the worker. Stats / aggregates inside `IProfiler`
  /// are unaffected -- this only thins the Chrome-trace JSON.
  /// Default: `Detailed` (no filtering).
  void SetMinLevel(ProfLevel level) noexcept
  {
    m_MinLevel.store(level, ::std::memory_order_relaxed);
  }

  /// Current minimum-level filter.
  ProfLevel GetMinLevel() const noexcept
  {
    return m_MinLevel.load(::std::memory_order_relaxed);
  }

  void Write(const ProfEvent& event) noexcept override;
  void WriteBatch(::gecko::Span<const ProfEvent> events) noexcept override;
  void Flush() noexcept override;

private:
  ::gecko::Unique<::gecko::platform::FileWriter> m_Writer {};
  bool m_First {true};
  u64 m_Time0Ns {0};

  std::mutex m_Mu {};
  std::condition_variable m_Cv {};
  std::vector<ProfEvent> m_Pending {};
  std::atomic<bool> m_Run {true};
  std::thread m_Worker {};
  std::atomic<ProfLevel> m_MinLevel {ProfLevel::Detailed};

  // Serialises every write to m_Writer (worker drain, Flush(), destructor).
  // Without this, concurrent DrainAndWrite calls from the worker and the
  // main thread produced double-comma corruption in the JSON.
  std::mutex m_WriteMu {};

  // Already-emitted thread_name metadata (TID -> done). Worker-only.
  std::vector<u32> m_NamedThreads {};

  void WorkerLoop() noexcept;
  void DrainAndWrite(std::vector<ProfEvent>& batch) noexcept;
  void EmitThreadNameOnce(u32 tid, const char* name) noexcept;
};

}  // namespace gecko::runtime
