#include "gecko/runtime/crash_safe_trace_profiler_sink.h"

#include "gecko/core/assert.h"
#include "gecko/platform/platform_io.h"
#include "private/chrome_trace_format.h"
#include "private/file_writer_format.h"

namespace gecko::runtime {

namespace {

using ::gecko::runtime::detail::WriteChromeTraceEvent;
using ::gecko::runtime::detail::WriteFmt;

}  // namespace

CrashSafeTraceProfilerSink::CrashSafeTraceProfilerSink(const char* path)
{
  GECKO_ASSERT(path && "Trace file path cannot be null");

  m_Writer = ::gecko::platform::OpenWrite(
      path, ::gecko::platform::WriteMode::Truncate);

  if (m_Writer)
  {
    m_Writer->WriteString("{\"traceEvents\":[]}");
    m_Writer->Flush();
    m_First = true;
    m_Time0Ns = 0;
    m_EventCount.store(0, std::memory_order_relaxed);
  }
}

CrashSafeTraceProfilerSink::~CrashSafeTraceProfilerSink()
{
  Unregister();
  if (m_Writer)
  {
    EnsureValidJson();
    m_Writer.reset();
  }
}

void CrashSafeTraceProfilerSink::Write(const ProfEvent& event) noexcept
{
  if (!m_Writer)
    return;

  WriteEvent(event);

  size_t count = m_EventCount.fetch_add(1, std::memory_order_relaxed) + 1;
  if (count % FLUSH_INTERVAL == 0)
    EnsureValidJson();
}

void CrashSafeTraceProfilerSink::WriteBatch(
    ::std::span<const ProfEvent> events) noexcept
{
  if (!m_Writer || events.empty())
    return;

  for (const ProfEvent& e : events)
    WriteEvent(e);

  m_EventCount.fetch_add(events.size(), std::memory_order_relaxed);
  EnsureValidJson();
}

void CrashSafeTraceProfilerSink::WriteEvent(const ProfEvent& event) noexcept
{
  if (m_Time0Ns == 0)
    m_Time0Ns = event.TimestampNs;

  // Seek back to overwrite the closing ]} and insert new event.
  m_Writer->Seek(-2, /*fromEnd=*/true);

  WriteSeparator();
  WriteChromeTraceEvent(m_Writer.get(), event, m_Time0Ns);
  m_Writer->WriteString("]}");
}

void CrashSafeTraceProfilerSink::WriteSeparator() noexcept
{
  if (!m_First)
    m_Writer->WriteString(",");
  m_First = false;
}

void CrashSafeTraceProfilerSink::EnsureValidJson() noexcept
{
  if (m_Writer)
    m_Writer->Flush();
}

void CrashSafeTraceProfilerSink::Flush() noexcept
{
  EnsureValidJson();
}

}  // namespace gecko::runtime
