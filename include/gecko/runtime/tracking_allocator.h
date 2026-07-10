#pragma once

/// @file
/// `TrackingAllocator` -- per-`Label` tracking `IAllocator`.

#include "gecko/core/array.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"

namespace gecko::runtime {

/// Per-label live-bytes / alloc / free counters.
struct MemLabelStats
{
  u64 LiveBytes {0};
  u64 Allocs {0};
  u64 Frees {0};
  Label StatsLabel {};
};

/// Maximum nesting depth for `PushLabel` / `PopLabel`.
constexpr u32 MaxLabelStackDepth = 64;

/// Per-label-tracking `IAllocator`. Uses `PlatformAlloc`/`PlatformFree`
/// directly with a `TrackingAllocMagic` header. Cross-allocator frees
/// are handled: `SystemAllocMagic` allocations (made before boot) are
/// forwarded to `PlatformFree`.
class TrackingAllocator final : public IAllocator
{
public:
  TrackingAllocator() noexcept;
  ~TrackingAllocator() noexcept;

  void* Alloc(u64 size, u32 alignment) noexcept override;
  void Free(void* ptr) noexcept override;

  void PushLabel(Label label) noexcept override;
  void PopLabel() noexcept override;
  Label CurrentLabel() const noexcept override;

  bool Init() noexcept override;
  void Shutdown() noexcept override;

  /// Optional profiler used to emit per-label memory counters.
  void SetProfiler(IProfiler* profiler) noexcept;

  /// Total bytes currently live across all labels.
  u64 TotalLiveBytes() const noexcept;

  /// Copy out the stats for `label`. Returns `false` if `label` has no
  /// recorded allocations.
  bool StatsFor(Label label, MemLabelStats& outStats) const;
  /// Snapshot every label's stats into a Gecko-owned array.
  [[nodiscard]] ::gecko::Array<MemLabelStats> Snapshot() const;
  /// Emit the current per-label counters as profiler events.
  void EmitCounters() noexcept;
  /// Reset all per-label counters back to zero (does not free memory).
  void ResetCounters() noexcept;

private:
  struct Impl;
  ::gecko::Unique<Impl> m_Impl;
};

}  // namespace gecko::runtime
