#include "private/ring_profiler.h"

#include "gecko/core/services/log.h"
#include "gecko/core/utility/time.h"

namespace gecko::runtime {

RingProfiler::RingProfiler(usize) noexcept
{}

bool RingProfiler::Init() noexcept
{
  m_Initialized = true;
  return true;
}

void RingProfiler::Shutdown() noexcept
{
  m_Initialized = false;
}

u64 RingProfiler::NowNs() const noexcept
{
  return MonotonicTimeNs();
}

void RingProfiler::SetMinLevel(ProfLevel level) noexcept
{
  LockGuard lock(m_Mutex);
  m_MinLevel = level;
}

ProfLevel RingProfiler::GetMinLevel() const noexcept
{
  LockGuard lock(m_Mutex);
  const ProfLevel level = m_MinLevel;
  return level;
}

bool RingProfiler::IsLevelEnabled(ProfLevel level) const noexcept
{
  return static_cast<u8>(level) <= static_cast<u8>(GetMinLevel());
}

void RingProfiler::Emit(const ProfEvent& event) noexcept
{
  if (!m_Initialized || !IsLevelEnabled(event.Level))
    return;
  LockGuard lock(m_Mutex);
  if (event.Kind == ProfEventKind::ZoneBegin)
  {
    for (OpenZone& zone : m_OpenZones)
    {
      if (!zone.Active)
      {
        zone = OpenZone {.StartNs = event.TimestampNs,
                         .NameHash = event.NameHash,
                         .ThreadId = event.ThreadId,
                         .Source = event.Source,
                         .Active = true};
        return;
      }
    }
    ++m_Diagnostics.OpenZoneOverflow;
  }
  else if (event.Kind == ProfEventKind::ZoneEnd)
  {
    FinishZone(event);
  }
}

void RingProfiler::FinishZone(const ProfEvent& event) noexcept
{
  OpenZone* match = nullptr;
  for (OpenZone& zone : m_OpenZones)
  {
    if (zone.Active && zone.NameHash == event.NameHash && zone.ThreadId == event.ThreadId &&
        zone.Source == event.Source && (match == nullptr || zone.StartNs > match->StartNs))
      match = &zone;
  }
  if (match == nullptr)
  {
    ++m_Diagnostics.DroppedEvents;
    return;
  }

  const u64 elapsed = event.TimestampNs >= match->StartNs ? event.TimestampNs - match->StartNs : 0;
  match->Active = false;
  StatsSlot* destination = nullptr;
  for (StatsSlot& slot : m_Stats)
  {
    if (slot.Active && slot.NameHash == event.NameHash && slot.Source == event.Source)
    {
      destination = &slot;
      break;
    }
    if (!slot.Active && destination == nullptr)
      destination = &slot;
  }
  if (destination == nullptr)
  {
    ++m_Diagnostics.StatsOverflow;
    return;
  }
  if (!destination->Active)
  {
    destination->Active = true;
    destination->Name = event.Name;
    destination->NameHash = event.NameHash;
    destination->Source = event.Source;
  }
  ScopeStats& stats = destination->Stats;
  stats.LastNs = elapsed;
  stats.MinNs = elapsed < stats.MinNs ? elapsed : stats.MinNs;
  stats.MaxNs = elapsed > stats.MaxNs ? elapsed : stats.MaxNs;
  stats.TotalNs += elapsed;
  ++stats.Count;
  stats.AverageNs = stats.TotalNs / stats.Count;
}

ScopeStats RingProfiler::GetStats(u32 nameHash, ProfSource source) const noexcept
{
  LockGuard lock(m_Mutex);
  ScopeStats result {};
  for (const StatsSlot& slot : m_Stats)
  {
    if (slot.Active && slot.NameHash == nameHash && slot.Source == source)
    {
      result = slot.Stats;
      break;
    }
  }
  return result;
}

void RingProfiler::ResetStats() noexcept
{
  LockGuard lock(m_Mutex);
  for (StatsSlot& slot : m_Stats)
    slot = {};
  m_Diagnostics = {};
}

void RingProfiler::ForEachScope(ForEachScopeFn callback, void* user) const noexcept
{
  if (callback == nullptr)
    return;
  LockGuard lock(m_Mutex);
  for (const StatsSlot& slot : m_Stats)
    if (slot.Active)
      callback(slot.Name, slot.NameHash, slot.Source, slot.Stats, user);
}

namespace {
struct DumpContext
{
  Label OutputLabel;
};

void DumpScope(const char* name, u32 hash, ProfSource source, const ScopeStats& stats, void* user) noexcept
{
  auto* context = static_cast<DumpContext*>(user);
  GECKO_INFO(context->OutputLabel,
             "{} {:<40} count={:>6} last={:>8.3f}ms min={:>8.3f}ms max={:>8.3f}ms avg={:>8.3f}ms hash={:#x}",
             source == ProfSource::GPU ? "[GPU]" : "[CPU]", name != nullptr ? name : "(unnamed)", stats.Count,
             stats.LastNs / 1.0e6, stats.MinNs / 1.0e6, stats.MaxNs / 1.0e6, stats.AverageNs / 1.0e6, hash);
}
}  // namespace

void RingProfiler::DumpStats(Label label) const noexcept
{
  GECKO_INFO(label, "----- Gecko profiler stats -----");
  DumpContext context {label};
  ForEachScope(DumpScope, &context);
}

ProfilerDiagnostics RingProfiler::GetDiagnostics() const noexcept
{
  LockGuard lock(m_Mutex);
  const ProfilerDiagnostics diagnostics = m_Diagnostics;
  return diagnostics;
}

}  // namespace gecko::runtime
