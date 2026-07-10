#pragma once

/// @file
/// `RingProfiler` -- lock-free `IProfiler` implementation backed by
/// a power-of-two ring buffer with an in-process aggregator and
/// optional per-scope rolling-window watch entries.

#include "gecko/core/ptr.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

/// Asynchronous profiler backed by a single MPSC ring. Writers emit
/// `ProfEvent`s; a consumer job drains the ring and forwards events to
/// every attached `IProfilerSink`. An in-process aggregator updates
/// per-scope min/max/last/count statistics on every `ZoneEnd`.
class RingProfiler final : public IProfiler
{
public:
  /// Construct with `capacityPow2` slots (must be a power of two).
  explicit RingProfiler(size_t capacityPow2 = 1u << 20) noexcept;
  RingProfiler() noexcept;
  ~RingProfiler();

  void Emit(const ProfEvent& event) noexcept override;
  u64 NowNs() const noexcept override;

  void SetMinLevel(ProfLevel level) noexcept override;
  ProfLevel GetMinLevel() const noexcept override;
  bool IsLevelEnabled(ProfLevel level) const noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  /// Pop a single event from the ring without dispatching to sinks.
  /// Mainly used by tests; production code drains via `Flush()` or the
  /// background consumer.
  bool TryPop(ProfEvent& event) noexcept;

  void AddSink(IProfilerSink* sink) noexcept override;
  void RemoveSink(IProfilerSink* sink) noexcept override;

  void SetTraceEnabled(bool enabled) noexcept override;
  bool IsTraceEnabled() const noexcept override;

  void SetDetailedSampleRate(u32 nthEvent) noexcept override;
  u32 GetDetailedSampleRate() const noexcept override;

  ScopeStats GetStats(u32 nameHash, ProfSource source = ProfSource::CPU) const noexcept override;
  void WatchScope(u32 nameHash, u32 windowSize = 256, ProfSource source = ProfSource::CPU) noexcept override;
  void UnwatchScope(u32 nameHash, ProfSource source = ProfSource::CPU) noexcept override;

  void ResetStats() noexcept override;
  void SetStatsResetIntervalMs(u32 ms) noexcept override;
  u32 GetStatsResetIntervalMs() const noexcept override;

  void ForEachScope(ForEachScopeFn fn, void* user) const noexcept override;
  void DumpStats(Label label) const noexcept override;

  u8 RegisterCategory(const char* name) noexcept override;
  void SetCategoryEnabled(u8 id, bool on) noexcept override;
  bool IsCategoryEnabled(u8 id) const noexcept override;
  u8 FindCategory(const char* name) const noexcept override;
  const char* GetCategoryName(u8 id) const noexcept override;
  ProfilerDiagnostics GetDiagnostics() const noexcept override;

  using IProfiler::GetStats;
  using IProfiler::UnwatchScope;
  using IProfiler::WatchScope;

  /// Drain all pending events to sinks synchronously.
  void Flush() noexcept;

  /// When `false`, `Emit()` will not auto-schedule the consumer drain;
  /// callers must invoke `Flush()` explicitly (or rely on `Shutdown`'s
  /// drain) to deliver events. Default: `true`. Useful for tests that
  /// want a deterministic ring state and for clients that prefer
  /// explicit-flush semantics over the background-drain heuristic.
  void SetAutoScheduleEnabled(bool enabled) noexcept;
  /// Whether auto-scheduling of the consumer drain is enabled.
  bool IsAutoScheduleEnabled() const noexcept;

private:
  struct Impl;
  ::gecko::Unique<Impl> m_Impl;

  void ProcessProfEvents() noexcept;
  void TryScheduleConsumerJob() noexcept;
  void ScheduleNextConsumerJob() noexcept;
  bool HasPendingEvents() const noexcept;

  void UpdateAggregator(const ProfEvent& ev) noexcept;
  void ResetAggregator() noexcept;

  static u64 MonotonicNowNs() noexcept;
};

/// Stable thread id for the calling thread, suitable for use as
/// `ProfEvent::ThreadId`.
u32 ThisThreadId() noexcept;

}  // namespace gecko::runtime
