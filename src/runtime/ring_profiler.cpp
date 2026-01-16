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

u64 RingProfiler::MonotonicNowNs() noexcept
{
  return MonotonicTimeNs();
}

RingProfiler::RingProfiler(size_t capacityPow2)
    : m_Ring(capacityPow2), m_Mask(capacityPow2 - 1), m_Head(0), m_Tail(0),
      m_Run(true), m_ProfilerLabel(labels::Profiler)
{
  GECKO_ASSERT(capacityPow2 > 0 &&
               "Ring buffer capacity must be greater than 0");

  // Ensure capacity is power of 2
  if ((capacityPow2 & (capacityPow2 - 1)) != 0)
  {
    m_Mask = (Bit(20)) - 1;
    m_Ring = std::vector<Slot>(Bit(20));
  }
  for (u64 i = 0; i < m_Ring.size(); ++i)
  {
    m_Ring[i].Sequence.store(i, std::memory_order_relaxed);
  }
}

RingProfiler::~RingProfiler()
{
  // Stop accepting new events
  m_Run.store(false, std::memory_order_relaxed);

  // Wait for any active consumer job to complete
  if (m_ConsumerJob.IsValid())
  {
    WaitForJob(m_ConsumerJob);
  }

  // Process any remaining events to ensure sinks get final data
  ProcessProfEvents();
}

u64 RingProfiler::NowNs() const noexcept
{
  return MonotonicNowNs();
}

void RingProfiler::Emit(const ProfEvent& event) noexcept
{
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

bool RingProfiler::Init() noexcept
{
  return true;
}

void RingProfiler::Shutdown() noexcept
{}

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
  if (!m_Run.load(std::memory_order_relaxed))
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
  if (HasPendingEvents())
  {
    ScheduleNextConsumerJob();
  }
}

void RingProfiler::TryScheduleConsumerJob() noexcept
{
  if (!m_Run.load(std::memory_order_relaxed))
    return;

  // Only schedule if there's no active consumer job
  if (m_ConsumerJob.IsValid() && !IsJobComplete(m_ConsumerJob))
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

}  // namespace gecko::runtime
