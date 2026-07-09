#include "gecko/runtime/immediate_logger.h"

#include "gecko/core/assert.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/utility/time.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <new>
#include <vector>

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

struct ImmediateLogger::Impl
{
  std::vector<ILogSink*> Sinks;
  std::mutex Mutex;
  LogLevel Level {LogLevel::Info};
  bool ThreadSafe {false};
};

ImmediateLogger::ImmediateLogger() noexcept : m_Impl(new (::std::nothrow) Impl())
{}

ImmediateLogger::~ImmediateLogger() noexcept = default;

void ImmediateLogger::AddSink(ILogSink* sink) noexcept
{
  if (!sink || !m_Impl)
    return;

  if (m_Impl->ThreadSafe)
  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->Sinks.push_back(sink);
  }
  else
  {
    m_Impl->Sinks.push_back(sink);
  }
}

void ImmediateLogger::RemoveSink(ILogSink* sink) noexcept
{
  if (!sink || !m_Impl)
    return;

  if (m_Impl->ThreadSafe)
  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    auto it = std::find(m_Impl->Sinks.begin(), m_Impl->Sinks.end(), sink);
    if (it != m_Impl->Sinks.end())
      m_Impl->Sinks.erase(it);
  }
  else
  {
    auto it = std::find(m_Impl->Sinks.begin(), m_Impl->Sinks.end(), sink);
    if (it != m_Impl->Sinks.end())
      m_Impl->Sinks.erase(it);
  }
}

void ImmediateLogger::LogV(LogLevel level, Label label, const char* fmt, va_list apIn) noexcept
{
  GECKO_ASSERT(fmt && "Format string cannot be null");
  if (!m_Impl)
    return;

  // Check log level filter
  if (static_cast<int>(level) < static_cast<int>(m_Impl->Level))
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
  if (m_Impl->ThreadSafe)
  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    for (auto* sink : m_Impl->Sinks)
    {
      if (sink)
        sink->Write(message);
    }
  }
  else
  {
    for (auto* sink : m_Impl->Sinks)
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

void ImmediateLogger::SetLevel(LogLevel level) noexcept
{
  if (m_Impl)
    m_Impl->Level = level;
}

LogLevel ImmediateLogger::Level() const noexcept
{
  return m_Impl ? m_Impl->Level : LogLevel::Info;
}

void ImmediateLogger::SetThreadSafe(bool on) noexcept
{
  if (m_Impl)
    m_Impl->ThreadSafe = on;
}

void ImmediateLogger::Flush() noexcept
{
  // For immediate logger, flush is a no-op since everything is written
  // immediately We could potentially flush the underlying sinks if they support
  // it
}

}  // namespace gecko::runtime
