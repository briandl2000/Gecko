#include "gecko/runtime/ring_logger.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"
#include "private/labels.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

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

struct RingLogger::Impl
{
  struct Entry
  {
    std::atomic<u64> Sequence {0};
    LogLevel Level {LogLevel::Info};
    Label EntryLabel {};
    u64 TimeNs {0};
    u32 ThreadId {0};
    char Text[512] {};

    Entry() = default;

    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
  };

  std::vector<Entry> Ring;
  size_t Capacity {4096};
  size_t Mask {0};
  std::atomic<u64> Head {0};
  std::atomic<u64> Tail {0};

  std::mutex SinkMu;
  std::mutex JobMu;
  std::vector<ILogSink*> Sinks;

  std::atomic<LogLevel> Level {LogLevel::Info};
  std::atomic<bool> Run {true};
  std::atomic<u64> Dropped {0};

  JobHandle ConsumerJob;
  Label LoggerLabel {labels::Logger};
  std::atomic<u64> LastScheduleNs {0};

  IProfiler* Profiler {nullptr};
};

RingLogger::RingLogger(size_t capacity) noexcept : m_Impl(new (::std::nothrow) Impl())
{
  GECKO_ASSERT(capacity > 0 && "Ring buffer capacity must be greater than 0");
  if (!m_Impl)
    return;

  m_Impl->Capacity = capacity;

  // Ensure capacity is power of 2
  if ((m_Impl->Capacity & (m_Impl->Capacity - 1)) != 0)
    m_Impl->Capacity = 4096;
}

RingLogger::RingLogger() noexcept : RingLogger(4096)
{}

RingLogger::~RingLogger()
{
  if (m_Impl)
    m_Impl->Run.store(false, std::memory_order_relaxed);
}

void RingLogger::AddSink(ILogSink* sink) noexcept
{
  if (sink && m_Impl)
  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    m_Impl->Sinks.push_back(sink);
  }
}

void RingLogger::RemoveSink(ILogSink* sink) noexcept
{
  if (!sink || !m_Impl)
    return;

  // Flush all pending work first to ensure no in-flight references
  Flush();

  // Now safe to remove the sink
  std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
  auto it = std::find(m_Impl->Sinks.begin(), m_Impl->Sinks.end(), sink);
  if (it != m_Impl->Sinks.end())
    m_Impl->Sinks.erase(it);
}

void RingLogger::LogV(LogLevel level, Label label, const char* fmt, va_list apIn) noexcept
{
  GECKO_ASSERT(fmt && "Format string cannot be null");
  if (!m_Impl)
    return;

  if (static_cast<int>(level) < static_cast<int>(m_Impl->Level.load(std::memory_order_relaxed)))
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

  if (!m_Impl->Run.load(std::memory_order_relaxed))
  {
    LogMessage message {
        .TimeNs = NowNs(), .Text = buffer, .MessageLabel = label, .ThreadId = ThreadId(), .Level = level};

    // Copy sinks vector to avoid holding lock during I/O
    std::vector<ILogSink*> sinks;
    {
      std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
      sinks = m_Impl->Sinks;
    }
    for (auto* sink : sinks)
    {
      if (sink)
        sink->Write(message);
    }
    return;
  }

  u64 position = m_Impl->Head.fetch_add(1, std::memory_order_acq_rel);
  Impl::Entry& entry = m_Impl->Ring[position & m_Impl->Mask];

  u64 sequence = entry.Sequence.load(std::memory_order_acquire);

  // Bounded retry to prevent infinite loop with single-threaded job systems
  constexpr int MaxRetries = 1000;
  int retries = 0;

  while (static_cast<i64>(sequence) - static_cast<i64>(position) != 0)
  {
    if (++retries > MaxRetries)
    {
      // Ring is full and we can't make progress - fallback to direct write
      // The slot we claimed is "lost" but we don't hang
      m_Impl->Dropped.fetch_add(1, std::memory_order_relaxed);

      LogMessage message {
          .TimeNs = NowNs(), .Text = buffer, .MessageLabel = label, .ThreadId = ThreadId(), .Level = level};

      std::vector<ILogSink*> sinks;
      {
        std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
        sinks = m_Impl->Sinks;
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

  // Try to schedule processing. The reentrancy guard lives inside
  // TryScheduleConsumerJob() itself - around the Submit() call - so if
  // Submit inline-runs ProcessLogEntries (e.g. NullJobSystem), the
  // re-entered Log call's scheduling attempt is detected and skipped.
  TryScheduleConsumerJob();
}

void RingLogger::ProcessLogEntries() noexcept
{
  if (!m_Impl || !m_Impl->Run.load(std::memory_order_acquire))
  {
    return;
  }

  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<ILogSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    sinks = m_Impl->Sinks;
  }

  LogMessage message {};
  const int maxBatchSize = 128;  // Process more entries per job to reduce job overhead

  for (int batch = 0; batch < maxBatchSize; ++batch)
  {
    u64 position = m_Impl->Tail.load(std::memory_order_relaxed);
    Impl::Entry& entry = m_Impl->Ring[position & m_Impl->Mask];

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

    entry.Sequence.store(position + m_Impl->Ring.size(), std::memory_order_release);
    m_Impl->Tail.store(position + 1, std::memory_order_relaxed);
  }

  // Handle dropped message reporting (reuse the sinks vector from above)
  u64 dropped = m_Impl->Dropped.exchange(0, std::memory_order_relaxed);
  if (dropped)
  {
    LogMessage dropMessage {.TimeNs = NowNs(),
                            .Text = nullptr,
                            .MessageLabel = m_Impl->LoggerLabel,
                            .ThreadId = ThreadId(),
                            .Level = LogLevel::Warn};
    char temp[128];
    std::snprintf(temp, sizeof(temp), "[Logger] dropped %llu messages", static_cast<unsigned long long>(dropped));
    dropMessage.Text = temp;

    for (auto* sink : sinks)
    {
      if (sink)
        sink->Write(dropMessage);
    }
  }

  // Always check if there are more entries to process.
  // Only reschedule if the logger is still running.
  if (m_Impl->Run.load(std::memory_order_acquire) && HasPendingEntries())
  {
    ScheduleNextConsumerJob();
  }
}

void RingLogger::TryScheduleConsumerJob() noexcept
{
  if (!m_Impl)
    return;

  // Reentrancy guard: With single-threaded job systems (like NullJobSystem),
  // Submit() runs the job immediately inline, which could cause infinite
  // recursion
  if (g_InsideRingLogger)
    return;

  // Fast path: check if we're still running without acquiring mutex
  if (!m_Impl->Run.load(std::memory_order_acquire))
    return;

  // Rate-limit scheduling to avoid job spam (check BEFORE mutex). Per-
  // instance: a function-local static would couple unrelated RingLogger
  // instances together.
  u64 now = NowNs();
  u64 lastTime = m_Impl->LastScheduleNs.load(std::memory_order_relaxed);

  // Don't schedule too frequently (at most every 100us)
  if (now - lastTime < 100000)  // 100 microseconds
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
    // Guard against re-entry from sinks that may log during processing.
    g_InsideRingLogger = true;
    ProcessLogEntries();
    g_InsideRingLogger = false;
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_Impl->JobMu);
    // Check m_Impl->Run again while holding the lock to prevent shutdown race
    if (!m_Impl->Run.load(std::memory_order_acquire))
      return;
    // Same reasoning: NullJobSystem inline-runs Submit, so set the guard
    // around the Submit call too.
    g_InsideRingLogger = true;
    m_Impl->ConsumerJob =
        jobSystem->Submit([this]() { ProcessLogEntries(); }, JobPriority::Normal, m_Impl->LoggerLabel);
    g_InsideRingLogger = false;
  }
}

void RingLogger::ScheduleNextConsumerJob() noexcept
{
  TryScheduleConsumerJob();
}

bool RingLogger::HasPendingEntries() const noexcept
{
  if (!m_Impl || m_Impl->Ring.empty())
    return false;

  u64 position = m_Impl->Tail.load(std::memory_order_relaxed);
  const Impl::Entry& entry = m_Impl->Ring[position & m_Impl->Mask];
  u64 sequence = entry.Sequence.load(std::memory_order_acquire);
  return static_cast<i64>(sequence) - static_cast<i64>(position + 1) == 0;
}

void RingLogger::Flush() noexcept
{
  GECKO_PROFILE_NAMED(labels::Logger, "RingLogger::Flush");
  if (!m_Impl)
    return;

  // Copy sinks vector once to avoid holding lock during I/O
  std::vector<ILogSink*> sinks;
  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    sinks = m_Impl->Sinks;
  }

  while (true)
  {
    bool processedAny = false;

    while (true)
    {
      u64 position = m_Impl->Tail.load(std::memory_order_relaxed);
      Impl::Entry& entry = m_Impl->Ring[position & m_Impl->Mask];
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
      entry.Sequence.store(position + m_Impl->Ring.size(), std::memory_order_release);
      m_Impl->Tail.store(position + 1, std::memory_order_relaxed);
      processedAny = true;
    }

    if (!processedAny)
      break;
  }
}

bool RingLogger::Init() noexcept
{
  if (!m_Impl)
    return false;

  // Allocate ring buffer now that allocator is available
  if (m_Impl->Ring.empty())
  {
    // Direct resize with default construction avoids moves
    m_Impl->Ring = std::vector<Impl::Entry>(m_Impl->Capacity);
    m_Impl->Mask = m_Impl->Capacity - 1;
    for (u64 i = 0; i < m_Impl->Capacity; ++i)
    {
      m_Impl->Ring[i].Sequence.store(i, std::memory_order_relaxed);
    }
  }

  m_Impl->Run.store(true, std::memory_order_relaxed);

  // JobSystem is now available during Logger initialization (Allocator ->
  // JobSystem -> Profiler -> Logger order) Start the initial consumer job
  // immediately
  ScheduleNextConsumerJob();

  return true;
}

void RingLogger::Shutdown() noexcept
{
  if (!m_Impl)
    return;

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

  {
    std::lock_guard<std::mutex> lk(m_Impl->SinkMu);
    decltype(m_Impl->Sinks)().swap(m_Impl->Sinks);
  }
}

void RingLogger::SetLevel(LogLevel level) noexcept
{
  if (m_Impl)
    m_Impl->Level.store(level, std::memory_order_relaxed);
}

LogLevel RingLogger::Level() const noexcept
{
  return m_Impl ? m_Impl->Level.load(std::memory_order_relaxed) : LogLevel::Info;
}

void RingLogger::SetProfiler(IProfiler* profiler) noexcept
{
  if (m_Impl)
    m_Impl->Profiler = profiler;
}

}  // namespace gecko::runtime
