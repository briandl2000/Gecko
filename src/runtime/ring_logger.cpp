#include "gecko/runtime/ring_logger.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#include "gecko/core/assert.h"
#include "gecko/core/jobs.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"
#include "gecko/core/thread.h"
#include "gecko/core/time.h"
#include "labels.h"

namespace gecko::runtime {

u64 RingLogger::NowNs() noexcept { return MonotonicTimeNs(); }

u32 RingLogger::ThreadId() noexcept { return HashThreadId(); }

RingLogger::RingLogger(size_t capacity) : m_LoggerLabel(labels::Logger) {
  GECKO_ASSERT(capacity > 0 && "Ring buffer capacity must be greater than 0");

  // Ensure capacity is power of 2
  if ((capacity & (capacity - 1)) != 0)
    capacity = 4096;

  m_Ring = std::vector<Entry>(capacity);
  m_Mask = capacity - 1;
  for (u64 i = 0; i < capacity; ++i) {
    m_Ring[i].Sequence.store(i, std::memory_order_relaxed);
  }
}

RingLogger::~RingLogger() { Shutdown(); }

void RingLogger::AddSink(ILogSink *sink) noexcept {
  GECKO_ASSERT(sink && "Cannot add null sink");

  std::lock_guard<std::mutex> lk(m_SinkMu);
  m_Sinks.push_back(sink);
}

void RingLogger::LogV(LogLevel level, Label label, const char *fmt,
                      va_list apIn) noexcept {
  GECKO_ASSERT(fmt && "Format string cannot be null");

  if (static_cast<int>(level) <
      static_cast<int>(m_Level.load(std::memory_order_relaxed)))
    return;

  char buffer[512];
  va_list ap;
  va_copy(ap, apIn);
  int n = std::vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);
  if (n < 0) {
    buffer[0] = '\0';
  }

  // If we're shutting down, don't enqueue into the ring buffer.
  // (Producers could otherwise stall if the consumer is stopped.)
  if (!m_Run.load(std::memory_order_relaxed)) {
    LogMessage message{level, label, NowNs(), ThreadId(), buffer};
    {
      std::lock_guard<std::mutex> lk(m_SinkMu);
      for (auto *sink : m_Sinks) {
        sink->Write(message);
      }
    }
    return;
  }

  u64 position = m_Head.fetch_add(1, std::memory_order_acq_rel);
  Entry &entry = m_Ring[position & m_Mask];

  u64 sequence = entry.Sequence.load(std::memory_order_acquire);
  while (static_cast<i64>(sequence) - static_cast<i64>(position) != 0) {
    // The ring is full for this slot. We must not "skip" positions, otherwise
    // the single consumer can get stuck waiting for an unpublished entry.
    // Try to drain a bit on the current thread to ensure forward progress.
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
  TryScheduleConsumerJob();
}

void RingLogger::ProcessLogEntries() noexcept {
  LogMessage message{};
  const int maxBatchSize =
      128; // Process more entries per job to reduce job overhead

  for (int batch = 0; batch < maxBatchSize; ++batch) {
    u64 position = m_Tail.load(std::memory_order_relaxed);
    Entry &entry = m_Ring[position & m_Mask];

    u64 sequence = entry.Sequence.load(std::memory_order_acquire);
    if (static_cast<i64>(sequence) - static_cast<i64>(position + 1) != 0)
      break;

    message.Level = entry.Level;
    message.MessageLabel = entry.EntryLabel;
    message.TimeNs = entry.TimeNs;
    message.ThreadId = entry.ThreadId;
    message.Text = entry.Text;

    {
      std::lock_guard<std::mutex> lk(m_SinkMu);
      for (auto *sink : m_Sinks)
        sink->Write(message);
    }

    // NOTE: Cannot use profiling here - would create circular dependency!
    // Logger depends on Profiler, so Logger cannot call Profiler functions.
    
    entry.Sequence.store(position + m_Ring.size(), std::memory_order_release);
    m_Tail.store(position + 1, std::memory_order_relaxed);
  }

  // Handle dropped message reporting
  u64 dropped = m_Dropped.exchange(0, std::memory_order_relaxed);
  if (dropped) {
    LogMessage dropMessage{LogLevel::Warn, m_LoggerLabel, NowNs(), ThreadId(),
                           nullptr};
    char temp[128];
    std::snprintf(temp, sizeof(temp), "[Logger] dropped %llu messages",
                  static_cast<unsigned long long>(dropped));
    dropMessage.Text = temp;
    std::lock_guard<std::mutex> lk(m_SinkMu);
    for (auto *sink : m_Sinks)
      sink->Write(dropMessage);
  }

  // Always check if there are more entries to process.
  // Only reschedule if the logger is still running.
  if (m_Run.load(std::memory_order_relaxed) && HasPendingEntries()) {
    ScheduleNextConsumerJob();
  }
}

void RingLogger::TryScheduleConsumerJob() noexcept {
  if (!m_Run.load(std::memory_order_relaxed))
    return;

  // Only schedule if there's no active consumer job
  if (m_ConsumerJob.IsValid() && !IsJobComplete(m_ConsumerJob))
    return;

  auto *jobSystem = GetJobSystem();
  if (!jobSystem) {
    // No job system available, process immediately on current thread
    ProcessLogEntries();
    return;
  }

  // Use normal priority for logger jobs and limit how often we schedule
  static std::atomic<u64> lastScheduleTime{0};
  u64 now = NowNs();
  u64 lastTime = lastScheduleTime.load(std::memory_order_relaxed);

  // Don't schedule too frequently (at most every 100µs) to avoid job spam
  if (now - lastTime < 100000) // 100 microseconds
  {
    return;
  }

  if (lastScheduleTime.compare_exchange_weak(lastTime, now,
                                             std::memory_order_relaxed)) {
    m_ConsumerJob = jobSystem->Submit([this]() { ProcessLogEntries(); },
                                      JobPriority::Normal, m_LoggerLabel);
  }
}

void RingLogger::ScheduleNextConsumerJob() noexcept {
  TryScheduleConsumerJob();
}

bool RingLogger::HasPendingEntries() const noexcept {
  u64 position = m_Tail.load(std::memory_order_relaxed);
  const Entry &entry = m_Ring[position & m_Mask];
  u64 sequence = entry.Sequence.load(std::memory_order_acquire);
  return static_cast<i64>(sequence) - static_cast<i64>(position + 1) == 0;
}

void RingLogger::Flush() noexcept {
  while (true) {
    bool processedAny = false;

    while (true) {
      u64 position = m_Tail.load(std::memory_order_relaxed);
      Entry &entry = m_Ring[position & m_Mask];
      u64 sequence = entry.Sequence.load(std::memory_order_acquire);
      if (static_cast<i64>(sequence) - static_cast<i64>(position + 1) != 0)
        break;

      LogMessage message{entry.Level, entry.EntryLabel, entry.TimeNs,
             entry.ThreadId,
                         entry.Text};
      {
        std::lock_guard<std::mutex> lk(m_SinkMu);
        for (auto *sink : m_Sinks)
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

bool RingLogger::Init() noexcept {
  m_Run.store(true, std::memory_order_relaxed);

  // JobSystem is now available during Logger initialization (Allocator ->
  // JobSystem -> Profiler -> Logger order) Start the initial consumer job
  // immediately
  ScheduleNextConsumerJob();

  return true;
}

void RingLogger::Shutdown() noexcept {
  m_Run.store(false, std::memory_order_relaxed);

  // Wait for any running consumer job to complete
  if (m_ConsumerJob.IsValid()) {
    WaitForJob(m_ConsumerJob);
  }

  // Process any remaining entries
  Flush();
}

} // namespace gecko::runtime
