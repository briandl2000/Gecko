#include "gecko/runtime/immediate_logger.h"

#include "gecko/core/assert.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace gecko::runtime {

// Reentrancy guard: Prevents logger from logging itself
// (e.g., if logger internals call logged functions)
thread_local bool g_InsideLogger = false;

u64 NowNs() noexcept
{
  return MonotonicTimeNs();
}

u32 ThreadId() noexcept
{
  return HashThreadId();
}

void ImmediateLogger::AddSinkImpl(ILogSink* sink) noexcept
{
  if (!sink)
    return;

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

void ImmediateLogger::RemoveSinkImpl(ILogSink* sink) noexcept
{
  if (!sink)
    return;

  if (m_ThreadSafe)
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = std::find(m_Sinks.begin(), m_Sinks.end(), sink);
    if (it != m_Sinks.end())
      m_Sinks.erase(it);
  }
  else
  {
    auto it = std::find(m_Sinks.begin(), m_Sinks.end(), sink);
    if (it != m_Sinks.end())
      m_Sinks.erase(it);
  }
}

void ImmediateLogger::LogV(LogLevel level, Label label, const char* fmt,
                           va_list apIn) noexcept
{
  GECKO_ASSERT(fmt && "Format string cannot be null");

  // Check log level filter
  if (static_cast<int>(level) < static_cast<int>(m_Level))
  {
    return;
  }

  // Format the message
  char buffer[512];
  va_list ap;
  va_copy(ap, apIn);
  int n = std::vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);

  if (n < 0)
    buffer[0] = '\0';

  // Create log message
  LogMessage message;
  message.Level = level;
  message.MessageLabel = label;
  message.TimeNs = NowNs();
  message.ThreadId = ThreadId();
  message.Text = buffer;

  // Write to all sinks immediately
  if (m_ThreadSafe)
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto* sink : m_Sinks)
    {
      if (sink)
        sink->Write(message);
    }
  }
  else
  {
    for (auto* sink : m_Sinks)
    {
      if (sink)
        sink->Write(message);
    }
  }
}

bool ImmediateLogger::Init() noexcept
{
  return true;
}

void ImmediateLogger::Shutdown() noexcept
{}

void ImmediateLogger::Flush() noexcept
{
  // For immediate logger, flush is a no-op since everything is written
  // immediately We could potentially flush the underlying sinks if they support
  // it
}

}  // namespace gecko::runtime
