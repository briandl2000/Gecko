#include "gecko/runtime/ring_profiler.h"

#include "gecko/core/assert.h"
#include "gecko/core/utility/bit.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"
#include "private/labels.h"

#include <algorithm>
#include <vector>

namespace gecko {

u32 ThisThreadId() noexcept
{
  return HashThreadId();
}
}  // namespace gecko

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

void RingProfiler::Emit(const ProfEvent& event) noexcept
{
  if (!m_Run.load(std::memory_order_relaxed))
    return;

  u64 pos = m_Head.fetch_add(1, std::memory_order_acq_rel);
  Slot& slot = m_Ring[pos & m_Mask];

  u64 sequence = slot.Sequence.load(std::memory_order_acquire);
  i64 diff = (i64)sequence - (i64)pos;
  if (diff == 0)
  {
    slot.ProfileEvent = event;  // copy event
    slot.Sequence.store(pos + 1, std::memory_order_release);

    // Try to schedule async processing
    TryScheduleConsumerJob();
  }
  else
  {
    // overflow — drop event (cheap fallback)
    // Optionally: back off or count drops.
  }
}

bool RingProfiler::TryPop(ProfEvent& event) noexcept
{
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
  if (sink)
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    auto it = std::find(m_Sinks.begin(), m_Sinks.end(), sink);
    if (it != m_Sinks.end())
    {
      m_Sinks.erase(it);
    }
  }
}

void RingProfiler::ProcessProfEvents() noexcept
{
  if (!m_Run.load(std::memory_order_acquire))
    return;

  ProfEvent event {};
  const int maxBatchSize = 128;  // Process events in batches for efficiency

  for (int batch = 0; batch < maxBatchSize; ++batch)
  {
    if (!TryPop(event))
      break;

    // Write to all sinks
    {
      std::lock_guard<std::mutex> lk(m_SinkMu);
      for (auto* sink : m_Sinks)
      {
        if (sink)
        {
          sink->Write(event);
        }
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
  JobHandle currentJob;
  {
    std::lock_guard<std::mutex> lock(m_JobMu);
    // Check m_Run while holding lock to synchronize with Shutdown
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

  // Use low priority for profiler jobs and limit scheduling frequency
  static std::atomic<u64> lastScheduleTime {0};
  u64 now = NowNs();
  u64 lastTime = lastScheduleTime.load(std::memory_order_relaxed);

  // Don't schedule too frequently (at most every 100µs) to avoid job spam
  if (now - lastTime < 100000)  // 100 microseconds
  {
    return;
  }

  if (lastScheduleTime.compare_exchange_weak(lastTime, now,
                                             std::memory_order_relaxed))
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
  if (m_Ring.empty())
  {
    // Direct resize with default construction avoids moves
    m_Ring = std::vector<Slot>(m_Capacity);
    for (u64 i = 0; i < m_Capacity; ++i)
    {
      m_Ring[i].Sequence.store(i, std::memory_order_relaxed);
    }
  }

  return true;
}

void RingProfiler::Shutdown() noexcept
{
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

  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    decltype(m_Sinks)().swap(m_Sinks);
  }
}

}  // namespace gecko::runtime
