#include "gecko/runtime/ring_logger.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"
#include "private/labels.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

namespace gecko::runtime {

// Reentrancy guard: Prevents logger from logging itself
thread_local bool g_InsideRingLogger = false;

u64 RingLogger::NowNs() noexcept
{
  return MonotonicTimeNs();
}

u32 RingLogger::ThreadId() noexcept
{
  return HashThreadId();
}

RingLogger::RingLogger(size_t capacity) noexcept
    : m_Capacity(capacity), m_LoggerLabel(labels::Logger)
{
  GECKO_ASSERT(capacity > 0 && "Ring buffer capacity must be greater than 0");

  // Ensure capacity is power of 2
  if ((m_Capacity & (m_Capacity - 1)) != 0)
    m_Capacity = 4096;
}

RingLogger::RingLogger() noexcept : m_LoggerLabel(labels::Logger)
{}

RingLogger::~RingLogger()
{
  m_Run.store(false, std::memory_order_relaxed);
}

void RingLogger::AddSink(ILogSink* sink) noexcept
{
  if (sink)
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    m_Sinks.push_back(sink);
  }
}

void RingLogger::RemoveSink(ILogSink* sink) noexcept
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

void RingLogger::LogV(LogLevel level, Label label, const char* fmt,
                      va_list apIn) noexcept
{
  GECKO_ASSERT(fmt && "Format string cannot be null");

  if (static_cast<int>(level) <
      static_cast<int>(m_Level.load(std::memory_order_relaxed)))
    return;

  char buffer[512];
  va_list ap;
  va_copy(ap, apIn);
  int n = std::vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);
  if (n < 0)
  {
    buffer[0] = '\0';
  }

  if (!m_Run.load(std::memory_order_relaxed))
  {
    LogMessage message {.TimeNs = NowNs(),
                        .Text = buffer,
                        .MessageLabel = label,
                        .ThreadId = ThreadId(),
                        .Level = level};

    // Copy sinks vector to avoid holding lock during I/O
    std::vector<ILogSink*> sinks;
    {
      std::lock_guard<std::mutex> lk(m_SinkMu);
      sinks = m_Sinks;
    }
    for (auto* sink : sinks)
    {
      if (sink)
        sink->Write(message);
    }
    return;
  }

  u64 position = m_Head.fetch_add(1, std::memory_order_acq_rel);
  Entry& entry = m_Ring[position & m_Mask];

  u64 sequence = entry.Sequence.load(std::memory_order_acquire);

  // Bounded retry to prevent infinite loop with single-threaded job systems
  constexpr int kMaxRetries = 1000;
  int retries = 0;

  while (static_cast<i64>(sequence) - static_cast<i64>(position) != 0)
  {
    if (++retries > kMaxRetries)
    {
      // Ring is full and we can't make progress - fallback to direct write
      // The slot we claimed is "lost" but we don't hang
      m_Dropped.fetch_add(1, std::memory_order_relaxed);

      LogMessage message {.TimeNs = NowNs(),
                          .Text = buffer,
                          .MessageLabel = label,
                          .ThreadId = ThreadId(),
                          .Level = level};

      std::vector<ILogSink*> sinks;
      {
        std::lock_guard<std::mutex> lk(m_SinkMu);
        sinks = m_Sinks;
      }
      for (auto* sink : sinks)
      {
        if (sink)
          sink->Write(message);
      }
      return;
    }

    // Try to drain on current thread to make progress
    ProcessLogEntries();
    std::this_thread::yield();
    sequence = entry.Sequence.load(std::memory_order_acquire);
  }

  entry.Level = level;
  entry.EntryLabel = label;
  entry.TimeNs = NowNs();
  entry.ThreadId = ThreadId();
  const std::size_t maxLen = sizeof(entry.Text);
  std::size_t len = std::min(std::strlen(buffer), maxLen - 1);

  std::copy_n(buffer, len, entry.Text);
  entry.Text[len] = '\0';

  entry.Sequence.store(position + 1, std::memory_order_release);

  // Try to schedule processing, but don't wait if job system is busy
  // Use reentrancy guard to prevent infinite recursion with single-threaded job systems
  if (!g_InsideRingLogger)
  {
    g_InsideRingLogger = true;
    TryScheduleConsumerJob();
    g_InsideRingLogger = false;
  }
}

void RingLogger::ProcessLogEntries() noexcept
{
  if (!m_Run.load(std::memory_order_acquire))
    return;

  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<ILogSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    sinks = m_Sinks;
  }

  LogMessage message {};
  const int maxBatchSize =
      128;  // Process more entries per job to reduce job overhead

  for (int batch = 0; batch < maxBatchSize; ++batch)
  {
    u64 position = m_Tail.load(std::memory_order_relaxed);
    Entry& entry = m_Ring[position & m_Mask];

    u64 sequence = entry.Sequence.load(std::memory_order_acquire);
    if (static_cast<i64>(sequence) - static_cast<i64>(position + 1) != 0)
      break;

    message.Level = entry.Level;
    message.MessageLabel = entry.EntryLabel;
    message.TimeNs = entry.TimeNs;
    message.ThreadId = entry.ThreadId;
    message.Text = entry.Text;

    for (auto* sink : sinks)
    {
      if (sink)
        sink->Write(message);
    }

    // NOTE: Cannot use profiling here - would create circular dependency!
    // Logger depends on Profiler, so Logger cannot call Profiler functions.

    entry.Sequence.store(position + m_Ring.size(), std::memory_order_release);
    m_Tail.store(position + 1, std::memory_order_relaxed);
  }

  // Handle dropped message reporting (reuse the sinks vector from above)
  u64 dropped = m_Dropped.exchange(0, std::memory_order_relaxed);
  if (dropped)
  {
    LogMessage dropMessage {.TimeNs = NowNs(),
                            .Text = nullptr,
                            .MessageLabel = m_LoggerLabel,
                            .ThreadId = ThreadId(),
                            .Level = LogLevel::Warn};
    char temp[128];
    std::snprintf(temp, sizeof(temp), "[Logger] dropped %llu messages",
                  static_cast<unsigned long long>(dropped));
    dropMessage.Text = temp;

    for (auto* sink : sinks)
    {
      if (sink)
        sink->Write(dropMessage);
    }
  }

  // Always check if there are more entries to process.
  // Only reschedule if the logger is still running.
  if (m_Run.load(std::memory_order_acquire) && HasPendingEntries())
  {
    ScheduleNextConsumerJob();
  }
}

void RingLogger::TryScheduleConsumerJob() noexcept
{
  // Reentrancy guard: With single-threaded job systems (like NullJobSystem),
  // Submit() runs the job immediately inline, which could cause infinite recursion
  if (g_InsideRingLogger)
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
    ProcessLogEntries();
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_JobMu);
    // Check m_Run again while holding the lock to prevent shutdown race
    if (!m_Run.load(std::memory_order_acquire))
      return;
    m_ConsumerJob = jobSystem->Submit([this]() { ProcessLogEntries(); },
                                      JobPriority::Normal, m_LoggerLabel);
  }
}

void RingLogger::ScheduleNextConsumerJob() noexcept
{
  TryScheduleConsumerJob();
}

bool RingLogger::HasPendingEntries() const noexcept
{
  u64 position = m_Tail.load(std::memory_order_relaxed);
  const Entry& entry = m_Ring[position & m_Mask];
  u64 sequence = entry.Sequence.load(std::memory_order_acquire);
  return static_cast<i64>(sequence) - static_cast<i64>(position + 1) == 0;
}

void RingLogger::Flush() noexcept
{
  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<ILogSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    sinks = m_Sinks;
  }

  while (true)
  {
    bool processedAny = false;

    while (true)
    {
      u64 position = m_Tail.load(std::memory_order_relaxed);
      Entry& entry = m_Ring[position & m_Mask];
      u64 sequence = entry.Sequence.load(std::memory_order_acquire);
      if (static_cast<i64>(sequence) - static_cast<i64>(position + 1) != 0)
        break;

      LogMessage message {.TimeNs = entry.TimeNs,
                          .Text = entry.Text,
                          .MessageLabel = entry.EntryLabel,
                          .ThreadId = entry.ThreadId,
                          .Level = entry.Level};

      for (auto* sink : sinks)
      {
        if (sink)
          sink->Write(message);
      }
      entry.Sequence.store(position + m_Ring.size(), std::memory_order_release);
      m_Tail.store(position + 1, std::memory_order_relaxed);
      processedAny = true;
    }

    if (!processedAny)
      break;
  }
}

bool RingLogger::Init() noexcept
{
  // Allocate ring buffer now that allocator is available
  if (m_Ring.empty())
  {
    // Direct resize with default construction avoids moves
    m_Ring = std::vector<Entry>(m_Capacity);
    m_Mask = m_Capacity - 1;
    for (u64 i = 0; i < m_Capacity; ++i)
    {
      m_Ring[i].Sequence.store(i, std::memory_order_relaxed);
    }
  }

  m_Run.store(true, std::memory_order_relaxed);

  // JobSystem is now available during Logger initialization (Allocator ->
  // JobSystem -> Profiler -> Logger order) Start the initial consumer job
  // immediately
  ScheduleNextConsumerJob();

  return true;
}

void RingLogger::Shutdown() noexcept
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
