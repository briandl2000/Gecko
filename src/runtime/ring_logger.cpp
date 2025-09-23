#include "gecko/runtime/ring_logger.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>

#include "categories.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"

namespace gecko::runtime {

  static inline u32 HashId()
  {
    auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return (u32)(id ^ (id >> 32));
  }

  u64 RingLogger::NowNs() noexcept 
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  u32 RingLogger::ThreadId() noexcept { return HashId(); }

  RingLogger::RingLogger(size_t capacity)
  {
    if ((capacity & (capacity - 1)) != 0) capacity = 4096;
    m_Ring = std::vector<Entry>(capacity);
    m_Mask = capacity - 1;
    for (u64 i = 0; i < capacity; ++i)
    {
      m_Ring[i].Sequence.store(i, std::memory_order_relaxed);
    }

    m_Consumer = std::thread([this]{ 
      ConsumerLoop(); 
    });
  }

  RingLogger::~RingLogger()
  {
    m_Run.store(false, std::memory_order_relaxed);
    m_ConsumerVariable.notify_all();
    if (m_Consumer.joinable()) m_Consumer.join();
  }

  void RingLogger::AddSink(ILogSink* sink) noexcept
  {
    std::lock_guard<std::mutex> lk(m_SinkMu);
    m_Sinks.push_back(sink);
  }

  void RingLogger::LogV(LogLevel level, Category category, const char* fmt, va_list apIn) noexcept
  {
    if (static_cast<int>(level) < static_cast<int>(m_Level.load(std::memory_order_relaxed))) return;

    char buffer[512];
    va_list ap;
    va_copy(ap, apIn);
    int n = std::vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    if (n < 0) { buffer[0] = '\0'; }

    u64 position = m_Head.fetch_add(1, std::memory_order_acq_rel);
    Entry& entry = m_Ring[position & m_Mask];

    u64 sequence = entry.Sequence.load(std::memory_order_acquire);
    if (static_cast<i64>(sequence) - static_cast<i64>(position) != 0)
    {
      m_Dropped.fetch_add(1, std::memory_order_relaxed) ;
      return;
    }

    entry.Level = level;
    entry.Category = category;
    entry.TimeNs = NowNs();
    entry.ThreadId = ThreadId();
    std::size_t maxLen = sizeof(entry.Text) - 1;
    std::size_t len = std::min(std::strlen(buffer), maxLen);

    std::copy_n(buffer, len, entry.Text);
    entry.Text[sizeof(entry.Text)-1] = '\n';

    entry.Sequence.store(position + 1, std::memory_order_release);

    m_ConsumerVariable.notify_one();

  }

  void RingLogger::ConsumerLoop() noexcept
  {
    LogMessage message {};
    while (m_Run.load(std::memory_order_relaxed))
    {
      bool any = false;
      while (true) 
      {
        u64 position = m_Tail.load(std::memory_order_relaxed);
        Entry& entry = m_Ring[position & m_Mask];
        
        u64 sequence = entry.Sequence.load(std::memory_order_acquire);
        if (static_cast<i64>(sequence) - static_cast<i64>(position + 1) != 0) break;

        message.Level = entry.Level;
        message.Category = entry.Category;
        message.TimeNs = entry.TimeNs;
        message.ThreadId = entry.ThreadId;
        message.Text = entry.Text;

        {
          std::lock_guard<std::mutex> lk(m_SinkMu);
          for (auto* sink : m_Sinks) sink->Write(message);
        }

        if (m_Profiler && (message.Level == LogLevel::Error || message.Level == LogLevel::Fatal))
        {
          GECKO_PROF_COUNTER(message.Category, "LogErrorCount", 1);
        }

        entry.Sequence.store(position + m_Ring.size(), std::memory_order_release);
        m_Tail.store(position + 1, std::memory_order_relaxed);
        
        any = true;
      }

      if (!any)
      {
        std::unique_lock<std::mutex> lk(m_ConsumerVariableMutex);
        m_ConsumerVariable.wait_for(lk, std::chrono::milliseconds(10));
      }

      u64 dropped = m_Dropped.exchange(0, std::memory_order_relaxed);
      if (dropped)
      {
        LogMessage dropMessage { LogLevel::Warn, categories::Logger, NowNs(), ThreadId(), nullptr };
        char temp[128];
        std::snprintf(temp, sizeof(temp), "[Logger] dropped %llu messages", static_cast<unsigned long long>(dropped));
        dropMessage.Text = temp;
        std::lock_guard<std::mutex> lk(m_SinkMu);
        for (auto* sink : m_Sinks) sink->Write(dropMessage);
      }
    }
  }


  void RingLogger::Flush() noexcept
  {
    bool again = false;

    while (again)
    {
      again = false;
      while (true)
      {
        u64 position = m_Tail.load(std::memory_order_relaxed);
        Entry& entry = m_Ring[position & m_Mask];
        u64 sequence = entry.Sequence.load(std::memory_order_acquire);
        if (static_cast<i64>(sequence) - static_cast<i64>(position + 1) != 0) break;

        LogMessage message { entry.Level, entry.Category, entry.TimeNs, entry.ThreadId, entry.Text };
        {
          std::lock_guard<std::mutex> lk(m_SinkMu);
          for (auto* sink : m_Sinks) sink->Write(message);
        }
        entry.Sequence.store(position + m_Ring.size(), std::memory_order_release);
        m_Tail.store(position + 1, std::memory_order_relaxed);
        again = true;
      }
    }
  }

}
