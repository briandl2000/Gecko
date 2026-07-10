#include "gecko/runtime/ring_profiler.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/utility/bit.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"
#include "private/labels.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace gecko::runtime {

// Reentrancy guard: Prevents profiler from profiling itself
// (e.g., if profiler internally calls a logged/profiled function)
thread_local bool g_InsideProfiler = false;

// Per-thread open-scope stack used by UpdateAggregator to pair
// ZoneBegin / ZoneEnd events without sharing state across threads.
// Implemented as a fixed-size array + count so the type is trivially
// destructible: avoids MinGW's historically buggy code path for
// `thread_local` containers with non-trivial destructors (which on the
// MSYS2 UCRT64 CI runner aborted runtime_tests.exe before Catch2 could
// even print its banner). Depth 64 is far above any realistic
// PROF_SCOPE nesting; overflow is silently dropped (we never produced
// useful data past that depth anyway).
struct OpenScopeFrame
{
  void* Owner;  // void* avoids pulling RingProfiler ptr type into the TU init
  u32 NameHash;
  u8 Source;
  u64 BeginTs;
};
constexpr ::std::size_t MaxOpenScopeDepth = 64;
struct OpenScopeStack
{
  ::std::array<OpenScopeFrame, MaxOpenScopeDepth> Frames {};
  ::std::size_t Count = 0;
};
static_assert(::std::is_trivially_destructible_v<OpenScopeStack>);
thread_local OpenScopeStack g_OpenScopeStack {};

u64 RingProfiler::MonotonicNowNs() noexcept
{
  return MonotonicTimeNs();
}

struct RingProfiler::Impl
{
  struct Slot
  {
    std::atomic<u64> Sequence {0};
    ProfEvent ProfileEvent {};

    Slot() = default;

    Slot(const Impl::Slot&) = delete;
    Impl::Slot& operator=(const Impl::Slot&) = delete;
    Slot(Impl::Slot&&) = delete;
    Impl::Slot& operator=(Impl::Slot&&) = delete;
  };

  std::vector<Impl::Slot> Ring {};
  size_t Capacity {1u << 20};
  size_t Mask {0};
  std::atomic<u64> Head {0};
  std::atomic<u64> Tail {0};

  std::vector<IProfilerSink*> Sinks {};
  std::mutex SinkMu {};

  std::atomic<bool> Run {true};
  std::mutex JobMu {};
  JobHandle ConsumerJob {};
  std::atomic<u64> LastScheduleNs {0};
  std::atomic<bool> AutoSchedule {true};
  Label ProfilerLabel {labels::Profiler};
  std::atomic<ProfLevel> MinLevel {ProfLevel::Detailed};
  std::atomic<u64> DroppedEvents {0};
  std::atomic<u64> ReentrantDrops {0};
  std::atomic<u64> AggregatorOverflow {0};

  std::atomic<bool> TraceEnabled {true};
  std::atomic<u32> DetailedSampleRate {1};
  std::atomic<u64> DetailedCounter {0};

  std::atomic<u32> StatsResetIntervalMs {1000};
  std::atomic<u64> LastStatsResetNs {0};

  static constexpr size_t AggregatorCapacity = 1024;
  struct AggSlot
  {
    std::atomic<u32> NameHash {0};
    std::atomic<u8> Source {0};
    std::atomic<u64> LastNs {0};
    std::atomic<u64> MinNs {~u64 {0}};
    std::atomic<u64> MaxNs {0};
    std::atomic<u32> Count {0};
    std::atomic<u32> WatchIdx {~u32 {0}};
    std::atomic<const char*> Name {nullptr};
  };
  std::vector<Impl::AggSlot> Aggregator {};

  struct WatchEntry
  {
    std::vector<u64> Samples;
    std::atomic<u32> Head {0};
    std::atomic<u32> Filled {0};
    std::mutex Mu;
  };
  std::vector<std::unique_ptr<WatchEntry>> Watch {};
  mutable std::mutex WatchMu {};

  static constexpr u8 CategoryCapacity = ProfMaxCategories;
  std::atomic<u64> CategoryMask {~u64 {0}};
  mutable std::mutex CategoryMu {};
  std::vector<std::string> CategoryNames {};
};

RingProfiler::RingProfiler(size_t capacityPow2) noexcept : m_Impl(new (::std::nothrow) Impl())
{
  GECKO_ASSERT(capacityPow2 > 0 && "Ring buffer capacity must be greater than 0");
  if (!m_Impl)
    return;

  m_Impl->Capacity = capacityPow2;
  m_Impl->Head.store(0, std::memory_order_relaxed);
  m_Impl->Tail.store(0, std::memory_order_relaxed);
  m_Impl->Run.store(true, std::memory_order_relaxed);
  m_Impl->ProfilerLabel = labels::Profiler;

  // Ensure capacity is power of 2
  if ((m_Impl->Capacity & (m_Impl->Capacity - 1)) != 0)
  {
    m_Impl->Capacity = Bit(20);
  }
  m_Impl->Mask = m_Impl->Capacity - 1;
}

RingProfiler::RingProfiler() noexcept : RingProfiler(1u << 20)
{}

RingProfiler::~RingProfiler()
{
  if (m_Impl)
    m_Impl->Run.store(false, std::memory_order_relaxed);
}

u64 RingProfiler::NowNs() const noexcept
{
  return MonotonicNowNs();
}

void RingProfiler::SetMinLevel(ProfLevel level) noexcept
{
  m_Impl->MinLevel.store(level, std::memory_order_relaxed);
}

ProfLevel RingProfiler::GetMinLevel() const noexcept
{
  return m_Impl->MinLevel.load(std::memory_order_relaxed);
}

bool RingProfiler::IsLevelEnabled(ProfLevel level) const noexcept
{
  return level <= m_Impl->MinLevel.load(std::memory_order_relaxed);
}

void RingProfiler::Emit(const ProfEvent& event) noexcept
{
  if (!m_Impl->Run.load(std::memory_order_relaxed)) [[unlikely]]
    return;

  // Guard against emitting before Init (m_Impl->Ring is empty)
  if (m_Impl->Ring.empty()) [[unlikely]]
    return;

  // Auto-reset stats on a timer (independent of FrameMark).
  if (u32 interval = m_Impl->StatsResetIntervalMs.load(std::memory_order_relaxed); interval > 0)
  {
    u64 nowNs = event.TimestampNs;
    u64 lastNs = m_Impl->LastStatsResetNs.load(std::memory_order_relaxed);
    if (lastNs == 0)
    {
      m_Impl->LastStatsResetNs.store(nowNs, std::memory_order_relaxed);
    }
    else if (nowNs - lastNs >= u64 {interval} * 1'000'000ULL)
    {
      // Try to claim the reset; only one Emit succeeds per interval.
      if (m_Impl->LastStatsResetNs.compare_exchange_strong(lastNs, nowNs, std::memory_order_acq_rel,
                                                           std::memory_order_relaxed))
        ResetAggregator();
    }
  }

  // Aggregator update for ALL levels (cheap CAS+store). FrameMark no
  // longer auto-resets -- apps can call ResetStats() manually if they want
  // per-frame reset semantics.
  if (event.Kind == ProfEventKind::ZoneBegin || event.Kind == ProfEventKind::ZoneEnd)
  {
    UpdateAggregator(event);
  }

  // Detailed sample-rate gate. Drop most Detailed events here (before they
  // enter the ring) without touching the aggregator (already updated
  // above) so HUD queries stay accurate while trace volume drops.
  if (event.Level == ProfLevel::Detailed &&
      (event.Kind == ProfEventKind::ZoneBegin || event.Kind == ProfEventKind::ZoneEnd))
  {
    u32 rate = m_Impl->DetailedSampleRate.load(std::memory_order_relaxed);
    if (rate == 0)
      return;
    if (rate > 1)
    {
      u64 c = m_Impl->DetailedCounter.fetch_add(1, std::memory_order_relaxed);
      if (c % rate != 0)
        return;
    }
  }

  u64 pos = m_Impl->Head.fetch_add(1, std::memory_order_acq_rel);
  Impl::Slot& slot = m_Impl->Ring[pos & m_Impl->Mask];

  u64 sequence = slot.Sequence.load(std::memory_order_acquire);
  i64 diff = (i64)sequence - (i64)pos;
  if (diff == 0) [[likely]]
  {
    slot.ProfileEvent = event;  // copy event
    slot.Sequence.store(pos + 1, std::memory_order_release);

    // Try to schedule async processing. The reentrancy guard lives inside
    // TryScheduleConsumerJob() itself - around the Submit() call - so that
    // if Submit inline-runs ProcessProfEvents (e.g. NullJobSystem), the
    // re-entered Emit's call here is detected and skipped.
    TryScheduleConsumerJob();
  }
  else
  {
    // overflow -- drop event (cheap fallback)
    m_Impl->DroppedEvents.fetch_add(1, std::memory_order_relaxed);
  }
}

bool RingProfiler::TryPop(ProfEvent& event) noexcept
{
  // Guard against being called after Shutdown() (e.g. trace-sink destructors
  // running after engine.reset() -- they Unregister, which calls Flush, which
  // calls TryPop). Shutdown swaps the backing storage out, so without this
  // we'd indexing into an empty vector.
  if (m_Impl->Ring.empty())
    return false;

  u64 pos = m_Impl->Tail.load(std::memory_order_relaxed);
  Impl::Slot& slot = m_Impl->Ring[pos & m_Impl->Mask];
  u64 sequence = slot.Sequence.load(std::memory_order_acquire);
  i64 diff = (i64)sequence - (i64)(pos + 1);
  if (diff == 0)
  {
    event = slot.ProfileEvent;
    slot.Sequence.store(pos + m_Impl->Ring.size(), std::memory_order_release);
    m_Impl->Tail.store(pos + 1, std::memory_order_relaxed);
    return true;
  }
  return false;
}

void RingProfiler::AddSink(IProfilerSink* sink) noexcept
{
  if (sink)
  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    m_Impl->Sinks.push_back(sink);
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
    std::lock_guard<std::mutex> lock(m_Impl->JobMu);
    jobToWait = m_Impl->ConsumerJob;
    m_Impl->ConsumerJob = JobHandle {};
  }
  if (jobToWait.IsValid())
    WaitForJob(jobToWait);

  // Flush all pending work first to ensure no in-flight references
  Flush();

  // Now safe to remove the sink
  std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
  auto it = std::find(m_Impl->Sinks.begin(), m_Impl->Sinks.end(), sink);
  if (it != m_Impl->Sinks.end())
    m_Impl->Sinks.erase(it);
}

void RingProfiler::Flush() noexcept
{
  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<IProfilerSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    sinks = m_Impl->Sinks;
  }

  const bool trace = m_Impl->TraceEnabled.load(std::memory_order_relaxed);

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
  if (!m_Impl->Run.load(std::memory_order_acquire))
    return;

  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<IProfilerSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    sinks = m_Impl->Sinks;
  }

  const bool trace = m_Impl->TraceEnabled.load(std::memory_order_relaxed);

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
  u64 dropped = m_Impl->DroppedEvents.exchange(0, std::memory_order_relaxed);
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
  if (HasPendingEvents() && m_Impl->Run.load(std::memory_order_acquire))
  {
    ScheduleNextConsumerJob();
  }
}

void RingProfiler::TryScheduleConsumerJob() noexcept
{
  // Reentrancy guard: With single-threaded job systems (like NullJobSystem),
  // Submit() runs the job inline on this thread, which calls
  // ProcessProfEvents -> Sink::Write -> may Emit -> here again. Bail on the
  // recursive entry so we don't infinitely re-submit.
  if (g_InsideProfiler)
    return;

  // Fast path: check if we're still running without acquiring mutex
  if (!m_Impl->Run.load(std::memory_order_acquire))
    return;

  // Test/explicit-flush hook: caller has opted out of auto-draining.
  if (!m_Impl->AutoSchedule.load(std::memory_order_relaxed))
    return;

  // Rate-limit scheduling to avoid job spam (check BEFORE mutex). Per-
  // instance timestamp - a function-local static would couple unrelated
  // RingProfiler instances together (this bit us in Release CI where two
  // back-to-back test cases ran their first Emit() inside the same 10us
  // window and the second one silently no-op'd).
  u64 now = NowNs();
  u64 lastTime = m_Impl->LastScheduleNs.load(std::memory_order_relaxed);

  // Don't schedule too frequently (at most every 10us)
  if (now - lastTime < 10000)  // 10 microseconds
    return;

  // Try to claim the scheduling slot atomically (still no mutex)
  if (!m_Impl->LastScheduleNs.compare_exchange_weak(lastTime, now, std::memory_order_relaxed))
    return;

  // Now we need to check if a job is already running - this needs the mutex
  JobHandle currentJob;
  {
    std::lock_guard<std::mutex> lock(m_Impl->JobMu);
    if (!m_Impl->Run.load(std::memory_order_acquire))
      return;
    currentJob = m_Impl->ConsumerJob;
  }

  // Only schedule if there's no active consumer job
  if (currentJob.IsValid() && !IsJobComplete(currentJob))
    return;

  auto* jobSystem = GetJobSystem();
  if (!jobSystem)
  {
    // No job system available, process immediately on current thread.
    // Set the reentrancy flag so any Emits issued from sinks during
    // ProcessProfEvents see g_InsideProfiler == true and skip their own
    // re-scheduling attempt.
    g_InsideProfiler = true;
    ProcessProfEvents();
    g_InsideProfiler = false;
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_Impl->JobMu);
    // Check m_Impl->Run again while holding the lock to prevent shutdown race
    if (!m_Impl->Run.load(std::memory_order_acquire))
      return;
    // Same reasoning as above: NullJobSystem inline-runs Submit, so set the
    // guard around the Submit call as well.
    g_InsideProfiler = true;
    m_Impl->ConsumerJob = jobSystem->Submit([this]() { ProcessProfEvents(); }, JobPriority::Low, m_Impl->ProfilerLabel);
    g_InsideProfiler = false;
  }
}

void RingProfiler::ScheduleNextConsumerJob() noexcept
{
  TryScheduleConsumerJob();
}

bool RingProfiler::HasPendingEvents() const noexcept
{
  u64 head = m_Impl->Head.load(std::memory_order_relaxed);
  u64 tail = m_Impl->Tail.load(std::memory_order_relaxed);
  return head != tail;
}

bool RingProfiler::Init() noexcept
{
  // Allocate ring buffer now that allocator is available
  // Use GECKO_PUSH_LABEL so the profiler's own memory is tracked under its
  // label
  if (m_Impl->Ring.empty())
  {
    GECKO_PUSH_LABEL(m_Impl->ProfilerLabel);
    // Direct resize with default construction avoids moves
    m_Impl->Ring = std::vector<Impl::Slot>(m_Impl->Capacity);
    for (u64 i = 0; i < m_Impl->Capacity; ++i)
    {
      m_Impl->Ring[i].Sequence.store(i, std::memory_order_relaxed);
    }
  }

  if (m_Impl->Aggregator.empty())
  {
    GECKO_PUSH_LABEL(m_Impl->ProfilerLabel);
    m_Impl->Aggregator = std::vector<Impl::AggSlot>(Impl::AggregatorCapacity);
  }

  if (m_Impl->CategoryNames.empty())
  {
    GECKO_PUSH_LABEL(m_Impl->ProfilerLabel);
    m_Impl->CategoryNames.reserve(Impl::CategoryCapacity);
    m_Impl->CategoryNames.push_back("default");  // category id 0
  }

  return true;
}

void RingProfiler::Shutdown() noexcept
{
  // Flush all pending events before shutdown
  Flush();

  // Drop any open-scope frames this profiler pushed onto the calling
  // thread's TLS stack. The stack is process-lifetime and shared across
  // tests/instances; without this, dangling frames owned by `this` would
  // bleed into the next RingProfiler created on this thread (and could
  // even match new frames if the allocator reuses our address).
  {
    ::std::size_t dst = 0;
    for (::std::size_t src = 0; src < g_OpenScopeStack.Count; ++src)
    {
      if (g_OpenScopeStack.Frames[src].Owner != this)
      {
        if (dst != src)
          g_OpenScopeStack.Frames[dst] = g_OpenScopeStack.Frames[src];
        ++dst;
      }
    }
    g_OpenScopeStack.Count = dst;
  }

  JobHandle jobToWait;
  {
    std::lock_guard<std::mutex> lock(m_Impl->JobMu);
    m_Impl->Run.store(false, std::memory_order_release);
    jobToWait = m_Impl->ConsumerJob;
    m_Impl->ConsumerJob = JobHandle {};
  }

  if (jobToWait.IsValid())
  {
    WaitForJob(jobToWait);
  }

  decltype(m_Impl->Ring)().swap(m_Impl->Ring);
  decltype(m_Impl->Aggregator)().swap(m_Impl->Aggregator);
  {
    std::lock_guard<std::mutex> lk(m_Impl->WatchMu);
    decltype(m_Impl->Watch)().swap(m_Impl->Watch);
  }
  {
    std::lock_guard<std::mutex> lk(m_Impl->CategoryMu);
    decltype(m_Impl->CategoryNames)().swap(m_Impl->CategoryNames);
  }

  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    decltype(m_Impl->Sinks)().swap(m_Impl->Sinks);
  }
}

ScopeStats RingProfiler::GetStats(u32 nameHash, ProfSource source) const noexcept
{
  if (m_Impl->Aggregator.empty() || nameHash == 0)
    return {};

  const u8 srcKey = static_cast<u8>(static_cast<u8>(source) + 1);
  constexpr size_t cap = Impl::AggregatorCapacity;
  size_t idx = nameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    const Impl::AggSlot& slot = m_Impl->Aggregator[(idx + probe) & (cap - 1)];
    u32 key = slot.NameHash.load(std::memory_order_acquire);
    if (key == 0)
      return {};
    if (key == nameHash && slot.Source.load(std::memory_order_relaxed) == srcKey)
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
        std::lock_guard<std::mutex> lk(m_Impl->WatchMu);
        if (wIdx < m_Impl->Watch.size() && m_Impl->Watch[wIdx])
        {
          Impl::WatchEntry& w = *m_Impl->Watch[wIdx];
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
    return ProfInvalidCategory;

  std::lock_guard<std::mutex> lk(m_Impl->CategoryMu);
  for (size_t i = 0; i < m_Impl->CategoryNames.size(); ++i)
  {
    if (m_Impl->CategoryNames[i] == name)
      return static_cast<u8>(i);
  }
  if (m_Impl->CategoryNames.size() >= Impl::CategoryCapacity)
    return ProfInvalidCategory;
  u8 id = static_cast<u8>(m_Impl->CategoryNames.size());
  // Copy into owned storage; callers are free to pass temporaries.
  m_Impl->CategoryNames.emplace_back(name);
  return id;
}

void RingProfiler::SetCategoryEnabled(u8 id, bool on) noexcept
{
  if (id >= Impl::CategoryCapacity)
    return;
  u64 bit = u64 {1} << id;
  if (on)
    m_Impl->CategoryMask.fetch_or(bit, std::memory_order_relaxed);
  else
    m_Impl->CategoryMask.fetch_and(~bit, std::memory_order_relaxed);
}

bool RingProfiler::IsCategoryEnabled(u8 id) const noexcept
{
  if (id >= Impl::CategoryCapacity)
    return false;
  return (m_Impl->CategoryMask.load(std::memory_order_relaxed) & (u64 {1} << id)) != 0;
}

u8 RingProfiler::FindCategory(const char* name) const noexcept
{
  if (!name)
    return ProfInvalidCategory;
  std::lock_guard<std::mutex> lk(m_Impl->CategoryMu);
  for (size_t i = 0; i < m_Impl->CategoryNames.size(); ++i)
  {
    if (m_Impl->CategoryNames[i] == name)
      return static_cast<u8>(i);
  }
  return ProfInvalidCategory;
}

const char* RingProfiler::GetCategoryName(u8 id) const noexcept
{
  std::lock_guard<std::mutex> lk(m_Impl->CategoryMu);
  if (id >= m_Impl->CategoryNames.size())
    return nullptr;
  return m_Impl->CategoryNames[id].c_str();
}

void RingProfiler::SetTraceEnabled(bool enabled) noexcept
{
  m_Impl->TraceEnabled.store(enabled, std::memory_order_relaxed);
}

bool RingProfiler::IsTraceEnabled() const noexcept
{
  return m_Impl->TraceEnabled.load(std::memory_order_relaxed);
}

void RingProfiler::SetDetailedSampleRate(u32 nthEvent) noexcept
{
  m_Impl->DetailedSampleRate.store(nthEvent, std::memory_order_relaxed);
}

u32 RingProfiler::GetDetailedSampleRate() const noexcept
{
  return m_Impl->DetailedSampleRate.load(std::memory_order_relaxed);
}

void RingProfiler::WatchScope(u32 nameHash, u32 windowSize, ProfSource source) noexcept
{
  if (m_Impl->Aggregator.empty() || nameHash == 0 || windowSize == 0)
    return;

  const u8 srcKey = static_cast<u8>(static_cast<u8>(source) + 1);
  constexpr size_t cap = Impl::AggregatorCapacity;
  size_t idx = nameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    Impl::AggSlot& slot = m_Impl->Aggregator[(idx + probe) & (cap - 1)];
    u32 expected = slot.NameHash.load(std::memory_order_acquire);
    if (expected == 0)
    {
      // Reserve the slot up-front so subsequent ZoneEnds find it.
      if (slot.NameHash.compare_exchange_strong(expected, nameHash, std::memory_order_acq_rel,
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
      std::lock_guard<std::mutex> lk(m_Impl->WatchMu);
      u32 wIdx = slot.WatchIdx.load(std::memory_order_relaxed);
      if (wIdx == ~u32 {0} || wIdx >= m_Impl->Watch.size() || !m_Impl->Watch[wIdx])
      {
        auto entry = std::make_unique<Impl::WatchEntry>();
        entry->Samples.assign(windowSize, 0);
        m_Impl->Watch.push_back(std::move(entry));
        wIdx = static_cast<u32>(m_Impl->Watch.size() - 1);
        slot.WatchIdx.store(wIdx, std::memory_order_relaxed);
      }
      else
      {
        Impl::WatchEntry& w = *m_Impl->Watch[wIdx];
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
  if (m_Impl->Aggregator.empty() || nameHash == 0)
    return;

  const u8 srcKey = static_cast<u8>(static_cast<u8>(source) + 1);
  constexpr size_t cap = Impl::AggregatorCapacity;
  size_t idx = nameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    Impl::AggSlot& slot = m_Impl->Aggregator[(idx + probe) & (cap - 1)];
    u32 key = slot.NameHash.load(std::memory_order_acquire);
    if (key == 0)
      return;
    if (key == nameHash && slot.Source.load(std::memory_order_relaxed) == srcKey)
    {
      slot.WatchIdx.store(~u32 {0}, std::memory_order_relaxed);
      return;
    }
  }
}

void RingProfiler::ResetStats() noexcept
{
  ResetAggregator();
  m_Impl->LastStatsResetNs.store(MonotonicNowNs(), std::memory_order_relaxed);
}

void RingProfiler::SetStatsResetIntervalMs(u32 ms) noexcept
{
  m_Impl->StatsResetIntervalMs.store(ms, std::memory_order_relaxed);
  m_Impl->LastStatsResetNs.store(MonotonicNowNs(), std::memory_order_relaxed);
}

u32 RingProfiler::GetStatsResetIntervalMs() const noexcept
{
  return m_Impl->StatsResetIntervalMs.load(std::memory_order_relaxed);
}

void RingProfiler::SetAutoScheduleEnabled(bool enabled) noexcept
{
  m_Impl->AutoSchedule.store(enabled, std::memory_order_relaxed);
}

bool RingProfiler::IsAutoScheduleEnabled() const noexcept
{
  return m_Impl->AutoSchedule.load(std::memory_order_relaxed);
}

void RingProfiler::ForEachScope(ForEachScopeFn fn, void* user) const noexcept
{
  if (!fn || m_Impl->Aggregator.empty())
    return;
  for (size_t i = 0; i < m_Impl->Aggregator.size(); ++i)
  {
    const Impl::AggSlot& slot = m_Impl->Aggregator[i];
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
void DumpCallback(const char* name, u32 hash, ProfSource source, const ScopeStats& s, void* user) noexcept
{
  auto* ctx = static_cast<DumpCtx*>(user);
  const char* tag = (source == ProfSource::GPU) ? "[GPU]" : "[CPU]";
  GECKO_INFO(ctx->OutLabel,
             "{} {:<40} count={:>5}  last={:>8.3f}ms  min={:>8.3f}ms  "
             "max={:>8.3f}ms  avg={:>8.3f}ms  hash={:#x}",
             tag, name ? name : "(unnamed)", s.Count, s.LastNs / 1.0e6, s.MinNs / 1.0e6, s.MaxNs / 1.0e6,
             s.AvgNs / 1.0e6, hash);
}
}  // namespace

void RingProfiler::DumpStats(Label label) const noexcept
{
  GECKO_INFO(label, "----- Profiler stats (interval={}ms, sample-rate=1/{}, trace={}) -----",
             m_Impl->StatsResetIntervalMs.load(std::memory_order_relaxed),
             m_Impl->DetailedSampleRate.load(std::memory_order_relaxed),
             m_Impl->TraceEnabled.load(std::memory_order_relaxed) ? "on" : "off");
  DumpCtx ctx {label};
  ForEachScope(&DumpCallback, &ctx);
  ProfilerDiagnostics d = GetDiagnostics();
  GECKO_INFO(label, "----- diagnostics: dropped={}, reentrant={}, agg-overflow={} -----", d.DroppedEvents,
             d.ReentrantDrops, d.AggregatorOverflow);
}

ProfilerDiagnostics RingProfiler::GetDiagnostics() const noexcept
{
  ProfilerDiagnostics d {};
  d.DroppedEvents = m_Impl->DroppedEvents.load(std::memory_order_relaxed);
  d.ReentrantDrops = m_Impl->ReentrantDrops.load(std::memory_order_relaxed);
  d.AggregatorOverflow = m_Impl->AggregatorOverflow.load(std::memory_order_relaxed);
  return d;
}

void RingProfiler::UpdateAggregator(const ProfEvent& ev) noexcept
{
  if (m_Impl->Aggregator.empty() || ev.NameHash == 0) [[unlikely]]
    return;

  const u8 srcKey = static_cast<u8>(static_cast<u8>(ev.Source) + 1);

  // Per-thread open-scope stack lives at namespace scope (see
  // OpenScopeFrame above). Begin pushes; End pops the matching frame and
  // computes duration. Frames carry the owning RingProfiler* so test
  // shutdowns / multiple instances don't corrupt each other's stacks.
  // Well-nested PROF_SCOPE usage hits the fast back-of-stack match; if a
  // thread interleaves scopes we walk the stack to find the match.
  if (ev.Kind == ProfEventKind::ZoneBegin)
  {
    if (g_OpenScopeStack.Count < MaxOpenScopeDepth) [[likely]]
    {
      g_OpenScopeStack.Frames[g_OpenScopeStack.Count++] = {this, ev.NameHash, srcKey, ev.TimestampNs};
    }
    return;
  }

  // ZoneEnd: locate matching open frame on the TLS stack.
  u64 beginTs = 0;
  bool matched = false;
  for (::std::size_t i = g_OpenScopeStack.Count; i > 0; --i)
  {
    const OpenScopeFrame& f = g_OpenScopeStack.Frames[i - 1];
    if (f.Owner == this && f.NameHash == ev.NameHash && f.Source == srcKey)
    {
      beginTs = f.BeginTs;
      // Compact: shift any frames above the match down by one.
      for (::std::size_t j = i - 1; j + 1 < g_OpenScopeStack.Count; ++j)
        g_OpenScopeStack.Frames[j] = g_OpenScopeStack.Frames[j + 1];
      --g_OpenScopeStack.Count;
      matched = true;
      break;
    }
  }
  if (!matched || ev.TimestampNs < beginTs) [[unlikely]]
    return;
  const u64 dur = ev.TimestampNs - beginTs;

  // Update / claim aggregator slot for this (NameHash, Source).
  constexpr size_t cap = Impl::AggregatorCapacity;
  size_t idx = ev.NameHash & (cap - 1);
  for (size_t probe = 0; probe < cap; ++probe)
  {
    Impl::AggSlot& slot = m_Impl->Aggregator[(idx + probe) & (cap - 1)];
    u32 expected = slot.NameHash.load(std::memory_order_acquire);
    if (expected == 0)
    {
      if (slot.NameHash.compare_exchange_strong(expected, ev.NameHash, std::memory_order_acq_rel,
                                                std::memory_order_acquire))
      {
        slot.Source.store(srcKey, std::memory_order_relaxed);
        slot.Name.store(ev.Name, std::memory_order_relaxed);
        expected = ev.NameHash;
      }
      else if (expected != ev.NameHash)
        continue;
    }
    if (expected == ev.NameHash)
    {
      // Source publish race: the claimer hasn't stored Source yet. If
      // we observe 0, try to publish our srcKey ourselves; this resolves
      // the gap atomically without losing events.
      u8 cur = slot.Source.load(std::memory_order_relaxed);
      if (cur == 0)
      {
        u8 zero = 0;
        slot.Source.compare_exchange_strong(zero, srcKey, std::memory_order_relaxed);
        cur = slot.Source.load(std::memory_order_relaxed);
      }
      if (cur != srcKey)
        continue;
      slot.LastNs.store(dur, std::memory_order_relaxed);
      u64 prevMin = slot.MinNs.load(std::memory_order_relaxed);
      while (dur < prevMin && !slot.MinNs.compare_exchange_weak(prevMin, dur, std::memory_order_relaxed))
      {}
      u64 prevMax = slot.MaxNs.load(std::memory_order_relaxed);
      while (dur > prevMax && !slot.MaxNs.compare_exchange_weak(prevMax, dur, std::memory_order_relaxed))
      {}
      slot.Count.fetch_add(1, std::memory_order_relaxed);

      // Push duration into watcher ring if scope is watched.
      u32 wIdx = slot.WatchIdx.load(std::memory_order_relaxed);
      if (wIdx != ~u32 {0})
      {
        std::lock_guard<std::mutex> lk(m_Impl->WatchMu);
        if (wIdx < m_Impl->Watch.size() && m_Impl->Watch[wIdx])
        {
          Impl::WatchEntry& w = *m_Impl->Watch[wIdx];
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
      return;
    }
  }
  m_Impl->AggregatorOverflow.fetch_add(1, std::memory_order_relaxed);
}

void RingProfiler::ResetAggregator() noexcept
{
  // Preserve scope identity (NameHash/Source/Name/WatchIdx) so watched
  // scopes keep accumulating across resets; only clear the per-window
  // counters. LastNs is intentionally NOT reset - it represents "the
  // most recent observation" and a reset that happens between two
  // zone-ends should not make HUD readers see 0 ms until the next
  // zone-end fires.
  for (auto& slot : m_Impl->Aggregator)
  {
    slot.MinNs.store(~u64 {0}, std::memory_order_relaxed);
    slot.MaxNs.store(0, std::memory_order_relaxed);
    slot.Count.store(0, std::memory_order_relaxed);
  }
  // Watcher rings are intentionally NOT cleared here. They model a rolling
  // window of "the last N samples" and should not be wiped by the 1 s
  // auto-reset timer - otherwise HUD readers that rely on AvgNs see 0 ms
  // every reset boundary until the ring re-fills.
}

}  // namespace gecko::runtime
