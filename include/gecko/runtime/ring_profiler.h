#pragma once

/// @file
/// `RingProfiler` -- lock-free `IProfiler` implementation backed by
/// a power-of-two ring buffer with an in-process aggregator and
/// optional per-scope rolling-window watch entries.

#include "gecko/core/services/jobs.h"
#include "gecko/core/services/profiler.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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
  struct Slot
  {
    std::atomic<u64> Sequence {0};
    ProfEvent ProfileEvent {};

    Slot() = default;

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;
    Slot(Slot&&) = delete;
    Slot& operator=(Slot&&) = delete;
  };

  std::vector<Slot> m_Ring {};
  size_t m_Capacity {1u << 20};
  size_t m_Mask {0};
  std::atomic<u64> m_Head {0};
  std::atomic<u64> m_Tail {0};

  std::vector<IProfilerSink*> m_Sinks {};
  std::mutex m_SinkMu {};

  // Async consumer system
  std::atomic<bool> m_Run {true};
  std::mutex m_JobMu {};  // Protects m_ConsumerJob
  JobHandle m_ConsumerJob {};
  // Per-instance rate-limit timestamp for consumer-job scheduling.
  // Must NOT be a function-level static: that would couple unrelated
  // RingProfiler instances (e.g. across test cases) and silently drop
  // schedule attempts that happen within the rate window of the previous
  // instance's last attempt.
  std::atomic<u64> m_LastScheduleNs {0};
  std::atomic<bool> m_AutoSchedule {true};
  Label m_ProfilerLabel {};
  std::atomic<ProfLevel> m_MinLevel {ProfLevel::Detailed};
  std::atomic<u64> m_DroppedEvents {0};
  std::atomic<u64> m_ReentrantDrops {0};
  std::atomic<u64> m_AggregatorOverflow {0};

  // Routing: when false, sinks receive nothing; aggregator still updates.
  std::atomic<bool> m_TraceEnabled {true};
  // Detailed-event sample rate. 0 = drop all, 1 = all, K>1 = 1/K.
  std::atomic<u32> m_DetailedSampleRate {1};
  // Per-scope-hash counter for sample-rate decisions (cheap mod).
  std::atomic<u64> m_DetailedCounter {0};

  // Stats reset cadence.
  std::atomic<u32> m_StatsResetIntervalMs {1000};
  std::atomic<u64> m_LastStatsResetNs {0};

  // Aggregator: 1024-entry open-addressing table keyed by name-hash + source.
  // Updated on ZoneEnd for *every* level (cheap: single CAS). Cleared by
  // ResetStats() (manual) and by an auto-timer (see SetStatsResetIntervalMs;
  // default 1 s). FrameMark is a passive marker and does NOT reset stats.
  // ScopeStats::LastNs and the per-WatchScope rolling rings persist across
  // resets so HUD readers never see 0 ms in the gap between reset and the
  // next ZoneEnd; only Min/Max/Count are windowed. Reads via GetStats() are
  // relaxed snapshots (not transactional).
  static constexpr size_t AggregatorCapacity = 1024;
  struct AggSlot
  {
    std::atomic<u32> NameHash {0};
    std::atomic<u8> Source {0};  // 0 = empty, otherwise (ProfSource+1)
    std::atomic<u64> LastNs {0};
    std::atomic<u64> MinNs {~u64 {0}};
    std::atomic<u64> MaxNs {0};
    std::atomic<u32> Count {0};
    // Watcher index into m_Watch (or ~0u if not watched).
    std::atomic<u32> WatchIdx {~u32 {0}};
    // Tracks the human-readable name of the last event that wrote here
    // (latched on first write). Used by ForEachScope / DumpStats.
    std::atomic<const char*> Name {nullptr};
  };
  std::vector<AggSlot> m_Aggregator {};

  // Watch entries: rolling-window samples for opt-in scopes.
  struct WatchEntry
  {
    std::vector<u64> Samples;
    std::atomic<u32> Head {0};
    std::atomic<u32> Filled {0};
    std::mutex Mu;
  };
  // Use unique_ptr because WatchEntry holds a mutex (non-movable).
  std::vector<std::unique_ptr<WatchEntry>> m_Watch {};
  mutable std::mutex m_WatchMu {};

  // Category state: 64 named slots, each with a name and an enabled bit
  // packed into m_CategoryMask. Names are stored as owned strings so
  // callers may pass non-static buffers - we copy on registration.
  static constexpr u8 CategoryCapacity = ProfMaxCategories;
  std::atomic<u64> m_CategoryMask {~u64 {0}};  // all enabled by default
  mutable std::mutex m_CategoryMu {};
  std::vector<std::string> m_CategoryNames {};

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
