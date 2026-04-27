#include "gecko/runtime/ring_profiler.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/utility/bit.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"
#include "private/labels.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace gecko::runtime {

// Reentrancy guard: Prevents profiler from profiling itself
// (e.g., if profiler internally calls a logged/profiled function)
thread_local bool g_InsideProfiler = false;

u64 RingProfiler::MonotonicNowNs() noexcept
{
  return MonotonicTimeNs();
}

RingProfiler::RingProfiler(size_t capacityPow2) noexcept
    : m_Capacity(capacityPow2), m_Head(0), m_Tail(0), m_Run(true),
      m_ProfilerLabel(labels::Profiler)
{
  GECKO_ASSERT(capacityPow2 > 0 &&
               "Ring buffer capacity must be greater than 0");

  // Ensure capacity is power of 2
  if ((m_Capacity & (m_Capacity - 1)) != 0)
  {
    m_Capacity = Bit(20);
  }
  m_Mask = m_Capacity - 1;
}

RingProfiler::RingProfiler() noexcept
    : m_Head(0), m_Tail(0), m_Run(true), m_ProfilerLabel(labels::Profiler)
{
  m_Mask = m_Capacity - 1;
}

RingProfiler::~RingProfiler()
{
  m_Run.store(false, std::memory_order_relaxed);
}

u64 RingProfiler::NowNs() const noexcept
{
  return MonotonicNowNs();
}

void RingProfiler::SetMinLevel(ProfLevel level) noexcept
{
  m_MinLevel.store(level, std::memory_order_relaxed);
}

ProfLevel RingProfiler::GetMinLevel() const noexcept
{
  return m_MinLevel.load(std::memory_order_relaxed);
}

bool RingProfiler::IsLevelEnabled(ProfLevel level) const noexcept
{
  return level <= m_MinLevel.load(std::memory_order_relaxed);
}

void RingProfiler::Emit(const ProfEvent& event) noexcept
{
  if (!m_Run.load(std::memory_order_relaxed)) [[unlikely]]
    return;

  // Guard against emitting before Init (m_Ring is empty)
  if (m_Ring.empty()) [[unlikely]]
    return;

  // Auto-reset stats on a timer (independent of FrameMark).
  if (u32 interval = m_StatsResetIntervalMs.load(std::memory_order_relaxed);
      interval > 0)
  {
    u64 nowNs = event.TimestampNs;
    u64 lastNs = m_LastStatsResetNs.load(std::memory_order_relaxed);
    if (lastNs == 0)
    {
      m_LastStatsResetNs.store(nowNs, std::memory_order_relaxed);
    }
    else if (nowNs - lastNs >= u64 {interval} * 1'000'000ULL)
    {
      // Try to claim the reset; only one Emit succeeds per interval.
      if (m_LastStatsResetNs.compare_exchange_strong(lastNs, nowNs,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed))
        ResetAggregator();
    }
  }

  // Aggregator update for ALL levels (cheap CAS+store). FrameMark no
  // longer auto-resets — apps can call ResetStats() manually if they want
  // per-frame reset semantics.
  if (event.Kind == ProfEventKind::ZoneBegin ||
      event.Kind == ProfEventKind::ZoneEnd)
  {
    UpdateAggregator(event);
  }

  // Detailed sample-rate gate. Drop most Detailed events here (before they
  // enter the ring) without touching the aggregator (already updated
  // above) so HUD queries stay accurate while trace volume drops.
  if (event.Level == ProfLevel::Detailed &&
      (event.Kind == ProfEventKind::ZoneBegin ||
       event.Kind == ProfEventKind::ZoneEnd))
  {
    u32 rate = m_DetailedSampleRate.load(std::memory_order_relaxed);
    if (rate == 0)
      return;
    if (rate > 1)
    {
      u64 c = m_DetailedCounter.fetch_add(1, std::memory_order_relaxed);
      if (c % rate != 0)
        return;
    }
  }

  u64 pos = m_Head.fetch_add(1, std::memory_order_acq_rel);
  Slot& slot = m_Ring[pos & m_Mask];

  u64 sequence = slot.Sequence.load(std::memory_order_acquire);
  i64 diff = (i64)sequence - (i64)pos;
  if (diff == 0) [[likely]]
  {
    slot.ProfileEvent = event;  // copy event
    slot.Sequence.store(pos + 1, std::memory_order_release);

    // Try to schedule async processing
    // Use reentrancy guard to prevent infinite recursion with single-threaded
    // job systems
    if (!g_InsideProfiler)
    {
      g_InsideProfiler = true;
      TryScheduleConsumerJob();
      g_InsideProfiler = false;
    }
  }
  else
  {
    // overflow — drop event (cheap fallback)
    m_DroppedEvents.fetch_add(1, std::memory_order_relaxed);
  }
}

bool RingProfiler::TryPop(ProfEvent& event) noexcept
{
  // Guard against being called after Shutdown() (e.g. trace-sink destructors
  // running after engine.reset() — they Unregister, which calls Flush, which
  // calls TryPop). Shutdown swaps the backing storage out, so without this
  // we'd indexing into an empty vector.
  if (m_Ring.empty())
    return false;

  u64 pos = m_Tail.load(std::memory_order_relaxed);
  Slot& slot = m_Ring[pos & m_Mask];
  u64 sequence = slot.Sequence.load(std::memory_order_acquire);
  i64 diff = (i64)sequence - (i64)(pos + 1);
  if (diff == 0)
  {
    event = slot.ProfileEvent;
    slot.Sequence.store(pos + m_Ring.size(), std::memory_order_release);
    m_Tail.store(pos + 1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

void RingProfiler::AddSink(IProfilerSink* sink) noexcept
{
  if (sink)
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    m_Sinks.push_back(sink);
  }
}

void RingProfiler::RemoveSink(IProfilerSink* sink) noexcept
{
  if (!sink)
    return;

  // Wait for any in-flight consumer job to finish: TryPop is SPSC and the
  // synchronous Flush() below would race against the worker, sometimes
  // dropping ZoneEnd events (causing trace viewers to show "[incomplete]"
  // zones).
  JobHandle jobToWait;
  {
    std::lock_guard<std::mutex> lock(m_JobMu);
    jobToWait = m_ConsumerJob;
    m_ConsumerJob = JobHandle {};
  }
  if (jobToWait.IsValid())
    WaitForJob(jobToWait);

  // Flush all pending work first to ensure no in-flight references
  Flush();

  // Now safe to remove the sink
  std::lock_guard<std::mutex> lk(m_SinkMu);
  auto it = std::find(m_Sinks.begin(), m_Sinks.end(), sink);
  if (it != m_Sinks.end())
    m_Sinks.erase(it);
}

void RingProfiler::Flush() noexcept
{
  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<IProfilerSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    sinks = m_Sinks;
  }

  const bool trace = m_TraceEnabled.load(std::memory_order_relaxed);

  // Process all pending events synchronously
  ProfEvent event {};
  while (TryPop(event))
  {
    if (!trace)
      continue;
    for (auto* sink : sinks)
    {
      if (sink)
        sink->Write(event);
    }
  }

  // Flush all sinks
  for (auto* sink : sinks)
  {
    if (sink)
      sink->Flush();
  }
}

void RingProfiler::ProcessProfEvents() noexcept
{
  if (!m_Run.load(std::memory_order_acquire))
    return;

  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<IProfilerSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    sinks = m_Sinks;
  }

  const bool trace = m_TraceEnabled.load(std::memory_order_relaxed);

  ProfEvent event {};
  const int maxBatchSize = 4096;  // Process events in batches for efficiency

  for (int batch = 0; batch < maxBatchSize; ++batch)
  {
    if (!TryPop(event))
      break;

    if (!trace)
      continue;

    for (auto* sink : sinks)
    {
      if (sink)
      {
        sink->Write(event);
      }
    }
  }

  // Report dropped events if any occurred (only when tracing).
  u64 dropped = m_DroppedEvents.exchange(0, std::memory_order_relaxed);
  if (dropped && trace)
  {
    // Emit a counter event to mark dropped events in the trace
    ProfEvent dropEvent {};
    dropEvent.Kind = ProfEventKind::Counter;
    dropEvent.TimestampNs = MonotonicNowNs();
    dropEvent.ThreadId = gecko::ThisThreadId();
    dropEvent.Name = "[Profiler] Dropped Events";
    dropEvent.Value = static_cast<u64>(dropped);

    for (auto* sink : sinks)
    {
      if (sink)
      {
        sink->Write(dropEvent);
      }
    }
  }

  // Continue processing if more events are pending
  if (HasPendingEvents() && m_Run.load(std::memory_order_acquire))
  {
    ScheduleNextConsumerJob();
  }
}

void RingProfiler::TryScheduleConsumerJob() noexcept
{
  // Reentrancy guard: With single-threaded job systems (like NullJobSystem),
  // Submit() runs the job immediately inline, which could cause infinite
  // recursion
  if (g_InsideProfiler)
    return;

  // Fast path: check if we're still running without acquiring mutex
  if (!m_Run.load(std::memory_order_acquire))
    return;

  // Rate-limit scheduling to avoid job spam (check BEFORE mutex)
  static std::atomic<u64> lastScheduleTime {0};
  u64 now = NowNs();
  u64 lastTime = lastScheduleTime.load(std::memory_order_relaxed);

  // Don't schedule too frequently (at most every 10µs)
  if (now - lastTime < 10000)  // 10 microseconds
    return;

  // Try to claim the scheduling slot atomically (still no mutex)
  if (!lastScheduleTime.compare_exchange_weak(lastTime, now,
                                              std::memory_order_relaxed))
    return;

  // Now we need to check if a job is already running - this needs the mutex
  JobHandle currentJob;
  {
    std::lock_guard<std::mutex> lock(m_JobMu);
    if (!m_Run.load(std::memory_order_acquire))
      return;
    currentJob = m_ConsumerJob;
  }

  // Only schedule if there's no active consumer job
  if (currentJob.IsValid() && !IsJobComplete(currentJob))
    return;

  auto* jobSystem = GetJobSystem();
  if (!jobSystem)
  {
    // No job system available, process immediately on current thread
    ProcessProfEvents();
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_JobMu);
    // Check m_Run again while holding the lock to prevent shutdown race
    if (!m_Run.load(std::memory_order_acquire))
      return;
    m_ConsumerJob = jobSystem->Submit([this]() { ProcessProfEvents(); },
                                      JobPriority::Low, m_ProfilerLabel);
  }
}

void RingProfiler::ScheduleNextConsumerJob() noexcept
{
  TryScheduleConsumerJob();
}

bool RingProfiler::HasPendingEvents() const noexcept
{
  u64 head = m_Head.load(std::memory_order_relaxed);
  u64 tail = m_Tail.load(std::memory_order_relaxed);
  return head != tail;
}

bool RingProfiler::Init() noexcept
{
  // Allocate ring buffer now that allocator is available
  // Use GECKO_PUSH_LABEL so the profiler's own memory is tracked under its
  // label
  if (m_Ring.empty())
  {
    GECKO_PUSH_LABEL(m_ProfilerLabel);
    // Direct resize with default construction avoids moves
    m_Ring = std::vector<Slot>(m_Capacity);
    for (u64 i = 0; i < m_Capacity; ++i)
    {
      m_Ring[i].Sequence.store(i, std::memory_order_relaxed);
    }
  }

  if (m_Aggregator.empty())
  {
    GECKO_PUSH_LABEL(m_ProfilerLabel);
    m_Aggregator = std::vector<AggSlot>(c_AggregatorCapacity);
  }

  if (m_CategoryNames.empty())
  {
    GECKO_PUSH_LABEL(m_ProfilerLabel);
    m_CategoryNames.reserve(c_CategoryCapacity);
    m_CategoryNames.push_back("default");  // category id 0
  }

  return true;
}

void RingProfiler::Shutdown() noexcept
{
  // Flush all pending events before shutdown
  Flush();

  JobHandle jobToWait;
  {
    std::lock_guard<std::mutex> lock(m_JobMu);
    m_Run.store(false, std::memory_order_release);
    jobToWait = m_ConsumerJob;
    m_ConsumerJob = JobHandle {};
  }

  if (jobToWait.IsValid())
  {
    WaitForJob(jobToWait);
  }

  decltype(m_Ring)().swap(m_Ring);
  decltype(m_Aggregator)().swap(m_Aggregator);
  {
    std::lock_guard<std::mutex> lk(m_WatchMu);
    decltype(m_Watch)().swap(m_Watch);
  }
  {
    std::lock_guard<std::mutex> lk(m_CategoryMu);
    decltype(m_CategoryNames)().swap(m_CategoryNames);
  }

  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    decltype(m_Sinks)().swap(m_Sinks);
  }
}

ScopeStats RingProfiler::GetStats(u32 nameHash,
                                  ProfSource source) const noexcept
{
  if (m_Aggregator.empty() || nameHash == 0)
    return {};

  const u8 srcKey = static_cast<u8>(static_cast<u8>(source) + 1);
  constexpr size_t cap = c_AggregatorCapacity;
  size_t idx = nameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    const AggSlot& slot = m_Aggregator[(idx + probe) & (cap - 1)];
    u32 key = slot.NameHash.load(std::memory_order_acquire);
    if (key == 0)
      return {};
    if (key == nameHash &&
        slot.Source.load(std::memory_order_relaxed) == srcKey)
    {
      ScopeStats s {};
      s.LastNs = slot.LastNs.load(std::memory_order_relaxed);
      s.MinNs = slot.MinNs.load(std::memory_order_relaxed);
      s.MaxNs = slot.MaxNs.load(std::memory_order_relaxed);
      s.Count = slot.Count.load(std::memory_order_relaxed);

      // If watched, compute average over the rolling window.
      u32 wIdx = slot.WatchIdx.load(std::memory_order_relaxed);
      if (wIdx != ~u32 {0})
      {
        std::lock_guard<std::mutex> lk(m_WatchMu);
        if (wIdx < m_Watch.size() && m_Watch[wIdx])
        {
          WatchEntry& w = *m_Watch[wIdx];
          std::lock_guard<std::mutex> wlk(w.Mu);
          u32 filled = w.Filled.load(std::memory_order_relaxed);
          if (filled > 0)
          {
            u64 sum = 0;
            for (u32 i = 0; i < filled; ++i)
              sum += w.Samples[i];
            s.AvgNs = sum / filled;
          }
        }
      }
      return s;
    }
  }
  return {};
}

u8 RingProfiler::RegisterCategory(const char* name) noexcept
{
  if (!name)
    return 0;

  std::lock_guard<std::mutex> lk(m_CategoryMu);
  for (size_t i = 0; i < m_CategoryNames.size(); ++i)
  {
    const char* existing = m_CategoryNames[i];
    if (existing && std::strcmp(existing, name) == 0)
      return static_cast<u8>(i);
  }
  if (m_CategoryNames.size() >= c_CategoryCapacity)
    return c_ProfInvalidCategory;
  u8 id = static_cast<u8>(m_CategoryNames.size());
  m_CategoryNames.push_back(name);
  return id;
}

void RingProfiler::SetCategoryEnabled(u8 id, bool on) noexcept
{
  if (id >= c_CategoryCapacity)
    return;
  u64 bit = u64 {1} << id;
  if (on)
    m_CategoryMask.fetch_or(bit, std::memory_order_relaxed);
  else
    m_CategoryMask.fetch_and(~bit, std::memory_order_relaxed);
}

bool RingProfiler::IsCategoryEnabled(u8 id) const noexcept
{
  if (id >= c_CategoryCapacity)
    return false;
  return (m_CategoryMask.load(std::memory_order_relaxed) & (u64 {1} << id)) !=
         0;
}

u8 RingProfiler::FindCategory(const char* name) const noexcept
{
  if (!name)
    return c_ProfInvalidCategory;
  std::lock_guard<std::mutex> lk(m_CategoryMu);
  for (size_t i = 0; i < m_CategoryNames.size(); ++i)
  {
    const char* existing = m_CategoryNames[i];
    if (existing && std::strcmp(existing, name) == 0)
      return static_cast<u8>(i);
  }
  return c_ProfInvalidCategory;
}

const char* RingProfiler::GetCategoryName(u8 id) const noexcept
{
  std::lock_guard<std::mutex> lk(m_CategoryMu);
  if (id >= m_CategoryNames.size())
    return nullptr;
  return m_CategoryNames[id];
}

void RingProfiler::SetTraceEnabled(bool enabled) noexcept
{
  m_TraceEnabled.store(enabled, std::memory_order_relaxed);
}

bool RingProfiler::IsTraceEnabled() const noexcept
{
  return m_TraceEnabled.load(std::memory_order_relaxed);
}

void RingProfiler::SetDetailedSampleRate(u32 nthEvent) noexcept
{
  m_DetailedSampleRate.store(nthEvent, std::memory_order_relaxed);
}

u32 RingProfiler::GetDetailedSampleRate() const noexcept
{
  return m_DetailedSampleRate.load(std::memory_order_relaxed);
}

void RingProfiler::WatchScope(u32 nameHash, u32 windowSize,
                              ProfSource source) noexcept
{
  if (m_Aggregator.empty() || nameHash == 0 || windowSize == 0)
    return;

  const u8 srcKey = static_cast<u8>(static_cast<u8>(source) + 1);
  constexpr size_t cap = c_AggregatorCapacity;
  size_t idx = nameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    AggSlot& slot = m_Aggregator[(idx + probe) & (cap - 1)];
    u32 expected = slot.NameHash.load(std::memory_order_acquire);
    if (expected == 0)
    {
      // Reserve the slot up-front so subsequent ZoneEnds find it.
      if (slot.NameHash.compare_exchange_strong(expected, nameHash,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire))
      {
        slot.Source.store(srcKey, std::memory_order_relaxed);
        expected = nameHash;
      }
      else if (expected != nameHash)
        continue;
    }
    if (expected == nameHash)
    {
      // (Re)allocate watcher entry.
      std::lock_guard<std::mutex> lk(m_WatchMu);
      u32 wIdx = slot.WatchIdx.load(std::memory_order_relaxed);
      if (wIdx == ~u32 {0} || wIdx >= m_Watch.size() || !m_Watch[wIdx])
      {
        auto entry = std::make_unique<WatchEntry>();
        entry->Samples.assign(windowSize, 0);
        m_Watch.push_back(std::move(entry));
        wIdx = static_cast<u32>(m_Watch.size() - 1);
        slot.WatchIdx.store(wIdx, std::memory_order_relaxed);
      }
      else
      {
        WatchEntry& w = *m_Watch[wIdx];
        std::lock_guard<std::mutex> wlk(w.Mu);
        w.Samples.assign(windowSize, 0);
        w.Head.store(0, std::memory_order_relaxed);
        w.Filled.store(0, std::memory_order_relaxed);
      }
      slot.Source.store(srcKey, std::memory_order_relaxed);
      return;
    }
  }
}

void RingProfiler::UnwatchScope(u32 nameHash, ProfSource source) noexcept
{
  if (m_Aggregator.empty() || nameHash == 0)
    return;

  const u8 srcKey = static_cast<u8>(static_cast<u8>(source) + 1);
  constexpr size_t cap = c_AggregatorCapacity;
  size_t idx = nameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    AggSlot& slot = m_Aggregator[(idx + probe) & (cap - 1)];
    u32 key = slot.NameHash.load(std::memory_order_acquire);
    if (key == 0)
      return;
    if (key == nameHash &&
        slot.Source.load(std::memory_order_relaxed) == srcKey)
    {
      slot.WatchIdx.store(~u32 {0}, std::memory_order_relaxed);
      return;
    }
  }
}

void RingProfiler::ResetStats() noexcept
{
  ResetAggregator();
  m_LastStatsResetNs.store(MonotonicNowNs(), std::memory_order_relaxed);
}

void RingProfiler::SetStatsResetIntervalMs(u32 ms) noexcept
{
  m_StatsResetIntervalMs.store(ms, std::memory_order_relaxed);
  m_LastStatsResetNs.store(MonotonicNowNs(), std::memory_order_relaxed);
}

u32 RingProfiler::GetStatsResetIntervalMs() const noexcept
{
  return m_StatsResetIntervalMs.load(std::memory_order_relaxed);
}

void RingProfiler::ForEachScope(ForEachScopeFn fn, void* user) const noexcept
{
  if (!fn || m_Aggregator.empty())
    return;
  for (size_t i = 0; i < m_Aggregator.size(); ++i)
  {
    const AggSlot& slot = m_Aggregator[i];
    u32 key = slot.NameHash.load(std::memory_order_acquire);
    if (key == 0)
      continue;
    u8 srcKey = slot.Source.load(std::memory_order_relaxed);
    if (srcKey == 0)
      continue;
    ProfSource source = static_cast<ProfSource>(srcKey - 1);
    ScopeStats s = GetStats(key, source);
    const char* name = slot.Name.load(std::memory_order_relaxed);
    fn(name, key, source, s, user);
  }
}

namespace {
struct DumpCtx
{
  Label OutLabel;
};
void DumpCallback(const char* name, u32 hash, ProfSource source,
                  const ScopeStats& s, void* user) noexcept
{
  auto* ctx = static_cast<DumpCtx*>(user);
  const char* tag = (source == ProfSource::GPU) ? "[GPU]" : "[CPU]";
  GECKO_INFO(ctx->OutLabel,
             "{} {:<40} count={:>5}  last={:>8.3f}ms  min={:>8.3f}ms  "
             "max={:>8.3f}ms  avg={:>8.3f}ms  hash={:#x}",
             tag, name ? name : "(unnamed)", s.Count, s.LastNs / 1.0e6,
             s.MinNs / 1.0e6, s.MaxNs / 1.0e6, s.AvgNs / 1.0e6, hash);
}
}  // namespace

void RingProfiler::DumpStats(Label label) const noexcept
{
  GECKO_INFO(
      label,
      "----- Profiler stats (interval={}ms, sample-rate=1/{}, trace={}) -----",
      m_StatsResetIntervalMs.load(std::memory_order_relaxed),
      m_DetailedSampleRate.load(std::memory_order_relaxed),
      m_TraceEnabled.load(std::memory_order_relaxed) ? "on" : "off");
  DumpCtx ctx {label};
  ForEachScope(&DumpCallback, &ctx);
  ProfilerDiagnostics d = GetDiagnostics();
  GECKO_INFO(
      label,
      "----- diagnostics: dropped={}, reentrant={}, agg-overflow={} -----",
      d.DroppedEvents, d.ReentrantDrops, d.AggregatorOverflow);
}

ProfilerDiagnostics RingProfiler::GetDiagnostics() const noexcept
{
  ProfilerDiagnostics d {};
  d.DroppedEvents = m_DroppedEvents.load(std::memory_order_relaxed);
  d.ReentrantDrops = m_ReentrantDrops.load(std::memory_order_relaxed);
  d.AggregatorOverflow = m_AggregatorOverflow.load(std::memory_order_relaxed);
  return d;
}

void RingProfiler::UpdateAggregator(const ProfEvent& ev) noexcept
{
  if (m_Aggregator.empty() || ev.NameHash == 0) [[unlikely]]
    return;

  const u8 srcKey = static_cast<u8>(static_cast<u8>(ev.Source) + 1);
  // Capacity is fixed at construction; use the constexpr so the compiler
  // can fold the (cap-1) mask into a constant.
  constexpr size_t cap = c_AggregatorCapacity;
  size_t idx = ev.NameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    AggSlot& slot = m_Aggregator[(idx + probe) & (cap - 1)];
    u32 expected = slot.NameHash.load(std::memory_order_acquire);
    if (expected == 0)
    {
      // Try to claim the slot.
      if (slot.NameHash.compare_exchange_strong(expected, ev.NameHash,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire))
      {
        slot.Source.store(srcKey, std::memory_order_relaxed);
        slot.Name.store(ev.Name, std::memory_order_relaxed);
        expected = ev.NameHash;
      }
      else if (expected != ev.NameHash)
        continue;
    }
    if (expected == ev.NameHash &&
        slot.Source.load(std::memory_order_relaxed) == srcKey)
    {
      if (ev.Kind == ProfEventKind::ZoneBegin)
      {
        slot.OpenBeginNs.store(ev.TimestampNs, std::memory_order_relaxed);
      }
      else if (ev.Kind == ProfEventKind::ZoneEnd)
      {
        u64 begin = slot.OpenBeginNs.load(std::memory_order_relaxed);
        if (begin != 0 && ev.TimestampNs >= begin)
        {
          u64 dur = ev.TimestampNs - begin;
          slot.LastNs.store(dur, std::memory_order_relaxed);
          u64 prevMin = slot.MinNs.load(std::memory_order_relaxed);
          if (dur < prevMin)
            slot.MinNs.store(dur, std::memory_order_relaxed);
          u64 prevMax = slot.MaxNs.load(std::memory_order_relaxed);
          if (dur > prevMax)
            slot.MaxNs.store(dur, std::memory_order_relaxed);
          slot.Count.fetch_add(1, std::memory_order_relaxed);
          slot.OpenBeginNs.store(0, std::memory_order_relaxed);

          // Push duration into watcher ring if scope is watched.
          u32 wIdx = slot.WatchIdx.load(std::memory_order_relaxed);
          if (wIdx != ~u32 {0})
          {
            std::lock_guard<std::mutex> lk(m_WatchMu);
            if (wIdx < m_Watch.size() && m_Watch[wIdx])
            {
              WatchEntry& w = *m_Watch[wIdx];
              std::lock_guard<std::mutex> wlk(w.Mu);
              if (!w.Samples.empty())
              {
                u32 cap2 = static_cast<u32>(w.Samples.size());
                u32 head = w.Head.load(std::memory_order_relaxed);
                w.Samples[head] = dur;
                w.Head.store((head + 1) % cap2, std::memory_order_relaxed);
                u32 filled = w.Filled.load(std::memory_order_relaxed);
                if (filled < cap2)
                  w.Filled.store(filled + 1, std::memory_order_relaxed);
              }
            }
          }
        }
      }
      return;
    }
  }
  m_AggregatorOverflow.fetch_add(1, std::memory_order_relaxed);
}

void RingProfiler::ResetAggregator() noexcept
{
  // Preserve scope identity (NameHash/Source/Name/WatchIdx) so watched
  // scopes keep accumulating across resets; only clear the per-window
  // counters. LastNs is intentionally NOT reset - it represents "the
  // most recent observation" and a reset that happens between two
  // zone-ends should not make HUD readers see 0 ms until the next
  // zone-end fires.
  for (auto& slot : m_Aggregator)
  {
    slot.MinNs.store(~u64 {0}, std::memory_order_relaxed);
    slot.MaxNs.store(0, std::memory_order_relaxed);
    slot.Count.store(0, std::memory_order_relaxed);
    slot.OpenBeginNs.store(0, std::memory_order_relaxed);
  }
  // Watcher rings are intentionally NOT cleared here. They model a rolling
  // window of "the last N samples" and should not be wiped by the 1 s
  // auto-reset timer - otherwise HUD readers that rely on AvgNs see 0 ms
  // every reset boundary until the ring re-fills.
}

}  // namespace gecko::runtime
