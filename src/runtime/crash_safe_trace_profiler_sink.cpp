#include "gecko/runtime/crash_safe_trace_profiler_sink.h"

#include "gecko/core/assert.h"
#include "gecko/platform/platform_io.h"

#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <vector>

namespace gecko::runtime {

namespace {

inline void WriteFmt(::gecko::platform::FileWriter* w, const char* fmt,
                     ...) noexcept
{
  char stack[1024];
  va_list ap;
  va_start(ap, fmt);
  va_list ap2;
  va_copy(ap2, ap);
  int n = std::vsnprintf(stack, sizeof(stack), fmt, ap);
  va_end(ap);
  if (n < 0)
  {
    va_end(ap2);
    return;
  }
  if (static_cast<::std::size_t>(n) < sizeof(stack))
  {
    va_end(ap2);
    w->WriteString(::std::string_view {stack, static_cast<::std::size_t>(n)});
    return;
  }
  ::std::vector<char> heap(static_cast<::std::size_t>(n) + 1);
  int n2 = std::vsnprintf(heap.data(), heap.size(), fmt, ap2);
  va_end(ap2);
  if (n2 < 0)
    return;
  w->WriteString(
      ::std::string_view {heap.data(), static_cast<::std::size_t>(n2)});
}

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

void CrashSafeTraceProfilerSink::WriteBatch(const ProfEvent* events,
                                            size_t count) noexcept
{
  if (!m_Writer || !events || count == 0)
    return;

  for (size_t i = 0; i < count; ++i)
    WriteEvent(events[i]);

  m_EventCount.fetch_add(count, std::memory_order_relaxed);
  EnsureValidJson();
}

void CrashSafeTraceProfilerSink::WriteEvent(const ProfEvent& event) noexcept
{
  if (m_Time0Ns == 0)
    m_Time0Ns = event.TimestampNs;

  // Seek back to overwrite the closing ]} and insert new event.
  m_Writer->Seek(-2, /*fromEnd=*/true);

  WriteSeparator();
  WriteJsonEventTo(m_Writer.get(), event, m_Time0Ns);
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

void CrashSafeTraceProfilerSink::WriteJsonEventTo(
    ::gecko::platform::FileWriter* w, const ProfEvent& event,
    u64 time0Ns) noexcept
{
  const double timeUs = (double)(event.TimestampNs - time0Ns) / 1000.0;
  const char* name = event.Name ? event.Name : "Unknown";
  const char* label = event.EventLabel.Name ? event.EventLabel.Name : "label";

  switch (event.Kind)
  {
  case ProfEventKind::ZoneBegin:
    WriteFmt(w,
             "{\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs,
             event.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    WriteFmt(w,
             "{\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs,
             event.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    WriteFmt(w,
             "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
             "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, timeUs, event.ThreadId);
    break;
  case ProfEventKind::Counter:
    WriteFmt(
        w,
        "{\"name\":\"%s\",\"cat\":\"%s "
        "(%llu)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
        name, label, (unsigned long long)event.EventLabel.Id, timeUs,
        (unsigned long long)event.Value);
    break;
  }
}

}  // namespace gecko::runtime
