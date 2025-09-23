#include "gecko/runtime/immediate_logger.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>

namespace gecko::runtime {

  static inline u32 HashId()
  {
    auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return (u32)(id ^ (id >> 32));
  }

  u64 ImmediateLogger::NowNs() noexcept 
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  u32 ImmediateLogger::ThreadId() noexcept 
  { 
    return HashId(); 
  }

  void ImmediateLogger::AddSink(ILogSink* sink) noexcept
  {
    std::lock_guard<std::mutex> lock(m_SinkMutex);
    m_Sinks.push_back(sink);
  }

  void ImmediateLogger::LogV(LogLevel level, Category category, const char* fmt, va_list apIn) noexcept
  {
    // Check log level filter
    if (static_cast<int>(level) < static_cast<int>(m_Level)) {
      return;
    }

    // Format the message
    char buffer[512];
    va_list ap;
    va_copy(ap, apIn);
    int n = std::vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    
    if (n < 0) {
      buffer[0] = '\0';
    }

    // Create log message
    LogMessage message;
    message.Level = level;
    message.Category = category;
    message.TimeNs = NowNs();
    message.ThreadId = ThreadId();
    message.Text = buffer;

    // Write to all sinks immediately
    std::lock_guard<std::mutex> lock(m_SinkMutex);
    for (auto* sink : m_Sinks) {
      if (sink) {
        sink->Write(message);
      }
    }
  }

  void ImmediateLogger::Flush() noexcept
  {
    // For immediate logger, flush is a no-op since everything is written immediately
    // We could potentially flush the underlying sinks if they support it
  }

}
