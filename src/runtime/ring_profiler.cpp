#include "gecko/runtime/ring_profiler.h"

#include "gecko/core/assert.h"
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
  if (!m_Run.load(std::memory_order_relaxed))
    return;

  // Guard against emitting before Init (m_Ring is empty)
  if (m_Ring.empty())
    return;

  // Aggregator update for Always-level zones; FrameMark resets it.
  if (event.Kind == ProfEventKind::FrameMark)
  {
    ResetAggregator();
  }
  else if (event.Level == ProfLevel::Always &&
           (event.Kind == ProfEventKind::ZoneBegin ||
            event.Kind == ProfEventKind::ZoneEnd))
  {
    UpdateAggregator(event);
  }

  u64 pos = m_Head.fetch_add(1, std::memory_order_acq_rel);
  Slot& slot = m_Ring[pos & m_Mask];

  u64 sequence = slot.Sequence.load(std::memory_order_acquire);
  i64 diff = (i64)sequence - (i64)pos;
  if (diff == 0)
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

  // Process all pending events synchronously
  ProfEvent event {};
  while (TryPop(event))
  {
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

  ProfEvent event {};
  const int maxBatchSize = 128;  // Process events in batches for efficiency

  for (int batch = 0; batch < maxBatchSize; ++batch)
  {
    if (!TryPop(event))
      break;

    for (auto* sink : sinks)
    {
      if (sink)
      {
        sink->Write(event);
      }
    }
  }

  // Report dropped events if any occurred
  u64 dropped = m_DroppedEvents.exchange(0, std::memory_order_relaxed);
  if (dropped)
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

  // Don't schedule too frequently (at most every 100µs)
  if (now - lastTime < 100000)  // 100 microseconds
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
    std::lock_guard<std::mutex> lk(m_CategoryMu);
    decltype(m_CategoryNames)().swap(m_CategoryNames);
  }

  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    decltype(m_Sinks)().swap(m_Sinks);
  }
}

ScopeStats RingProfiler::GetStats(u32 nameHash) const noexcept
{
  if (m_Aggregator.empty() || nameHash == 0)
    return {};

  const size_t cap = m_Aggregator.size();
  size_t idx = nameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    const AggSlot& slot = m_Aggregator[(idx + probe) & (cap - 1)];
    u32 key = slot.NameHash.load(std::memory_order_acquire);
    if (key == 0)
      return {};
    if (key == nameHash)
    {
      ScopeStats s {};
      s.LastNs = slot.LastNs.load(std::memory_order_relaxed);
      s.MinNs = slot.MinNs.load(std::memory_order_relaxed);
      s.MaxNs = slot.MaxNs.load(std::memory_order_relaxed);
      s.Count = slot.Count.load(std::memory_order_relaxed);
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
  if (m_Aggregator.empty() || ev.NameHash == 0)
    return;

  const size_t cap = m_Aggregator.size();
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
        expected = ev.NameHash;
      else if (expected != ev.NameHash)
        continue;
    }
    if (expected == ev.NameHash)
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
        }
      }
      return;
    }
  }
  m_AggregatorOverflow.fetch_add(1, std::memory_order_relaxed);
}

void RingProfiler::ResetAggregator() noexcept
{
  for (auto& slot : m_Aggregator)
  {
    slot.NameHash.store(0, std::memory_order_relaxed);
    slot.LastNs.store(0, std::memory_order_relaxed);
    slot.MinNs.store(~u64 {0}, std::memory_order_relaxed);
    slot.MaxNs.store(0, std::memory_order_relaxed);
    slot.Count.store(0, std::memory_order_relaxed);
    slot.OpenBeginNs.store(0, std::memory_order_relaxed);
  }
}

}  // namespace gecko::runtime
