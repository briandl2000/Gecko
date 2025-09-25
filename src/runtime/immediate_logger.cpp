#include "gecko/runtime/immediate_logger.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "gecko/core/assert.h"
#include "gecko/core/thread.h"
#include "gecko/core/time.h"

namespace gecko::runtime {

  u64 NowNs() noexcept 
  {
    return MonotonicTimeNs();
  }

  u32 ThreadId() noexcept 
  { 
    return HashThreadId(); 
  }

  void ImmediateLogger::AddSink(ILogSink* sink) noexcept
  {
    GECKO_ASSERT(sink && "Cannot add null sink");
    
    if (m_ThreadSafe) 
    { 
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Sinks.push_back(sink);
    }
    else 
    {
      m_Sinks.push_back(sink);
    }
  }

  void ImmediateLogger::LogV(LogLevel level, Category category, const char* fmt, va_list apIn) noexcept
  {
    GECKO_ASSERT(fmt && "Format string cannot be null");
    
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
    
    if (n < 0) buffer[0] = '\0'; 

    // Create log message
    LogMessage message;
    message.Level = level;
    message.Cat = category;
    message.TimeNs = NowNs();
    message.ThreadId = ThreadId();
    message.Text = buffer;

    // Write to all sinks immediately
    if (m_ThreadSafe)
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      for (auto* sink : m_Sinks) sink->Write(message);
    }
    else 
    {
      for (auto* sink : m_Sinks) sink->Write(message); 
    }
  }

  bool ImmediateLogger::Init() noexcept 
  {
    return true;
  }

  void ImmediateLogger::Shutdown() noexcept 
  {

  }

  void ImmediateLogger::Flush() noexcept
  {
    // For immediate logger, flush is a no-op since everything is written immediately
    // We could potentially flush the underlying sinks if they support it
  }

}
