#pragma once

#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

class RingProfiler final
{
public:
  explicit RingProfiler(usize ignoredCapacity = 0) noexcept;

  void Emit(const ProfEvent& event) noexcept;
  u64 NowNs() const noexcept;
  void SetMinLevel(ProfLevel level) noexcept;
  ProfLevel GetMinLevel() const noexcept;
  bool IsLevelEnabled(ProfLevel level) const noexcept;
  ScopeStats GetStats(u32 nameHash, ProfSource source = ProfSource::CPU) const noexcept;
  void ResetStats() noexcept;
  void ForEachScope(ForEachScopeFn callback, void* user) const noexcept;
  void DumpStats(Label label) const noexcept;
  ProfilerDiagnostics GetDiagnostics() const noexcept;
  bool Init() noexcept;
  void Shutdown() noexcept;

private:
  static constexpr u32 StatsCapacity = 1024;
  static constexpr u32 OpenZoneCapacity = 2048;

  struct StatsSlot
  {
    const char* Name {nullptr};
    u32 NameHash {0};
    ProfSource Source {ProfSource::CPU};
    ScopeStats Stats {};
    bool Active {false};
  };

  struct OpenZone
  {
    u64 StartNs {0};
    u32 NameHash {0};
    u32 ThreadId {0};
    ProfSource Source {ProfSource::CPU};
    bool Active {false};
  };

  void Lock() const noexcept;
  void Unlock() const noexcept;
  void FinishZone(const ProfEvent& event) noexcept;

  StatsSlot m_Stats[StatsCapacity] {};
  OpenZone m_OpenZones[OpenZoneCapacity] {};
  mutable u32 m_Lock {0};
  ProfLevel m_MinLevel {ProfLevel::Detailed};
  ProfilerDiagnostics m_Diagnostics {};
  bool m_Initialized {false};
};

}  // namespace gecko::runtime
