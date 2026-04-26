#include "gecko/runtime/trace_file_sink.h"

#include "gecko/core/assert.h"
#include "gecko/platform/platform_io.h"
#include "private/file_writer_format.h"

namespace gecko::runtime {

namespace {

using ::gecko::runtime::detail::WriteFmt;

}  // namespace

TraceFileSink::TraceFileSink(const char* path)
{
  GECKO_ASSERT(path && "Trace file path cannot be null");

  m_Writer = ::gecko::platform::OpenWrite(
      path, ::gecko::platform::WriteMode::Truncate);

  if (m_Writer)
  {
    m_Writer->WriteString("{\"traceEvents\":[\n");
    m_First = true;
    m_Time0Ns = 0;
    m_BufferedEvents.reserve(1000);
  }
}

TraceFileSink::~TraceFileSink()
{
  Unregister();
  if (m_Writer)
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    FlushBufferedEvents();
    m_Writer->WriteString("]}\n");
    m_Writer.reset();
  }
}

void TraceFileSink::Write(const ProfEvent& event) noexcept
{
  if (!m_Writer)
    return;

  std::lock_guard<std::mutex> lock(m_Mutex);

  if (m_Time0Ns == 0)
    m_Time0Ns = event.TimestampNs;

  m_BufferedEvents.push_back(event);

  if (m_BufferedEvents.size() >= 100)
    FlushBufferedEvents();
}

void TraceFileSink::WriteBatch(const ProfEvent* events, size_t count) noexcept
{
  if (!m_Writer || !events)
    return;

  for (size_t i = 0; i < count; ++i)
    Write(events[i]);
}

void TraceFileSink::Flush() noexcept
{
  if (m_Writer)
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    FlushBufferedEvents();
    m_Writer->Flush();
  }
}

void TraceFileSink::WriteJsonEvent(const ProfEvent& event) noexcept
{
  const double timeUs = (double)(event.TimestampNs - m_Time0Ns) / 1000.0;
  const char* name = event.Name ? event.Name : "Unknown";
  const char* label = event.EventLabel.Name ? event.EventLabel.Name : "label";

  if (!m_First)
    m_Writer->WriteString(",\n");
  m_First = false;

  switch (event.Kind)
  {
  case ProfEventKind::ZoneBegin:
    WriteFmt(m_Writer.get(),
             "  {\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs,
             event.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    WriteFmt(m_Writer.get(),
             "  {\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs,
             event.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    WriteFmt(m_Writer.get(),
             "  "
             "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
             "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, timeUs, event.ThreadId);
    break;
  case ProfEventKind::Counter:
    WriteFmt(
        m_Writer.get(),
        "  {\"name\":\"%s\",\"cat\":\"%s "
        "(%llu)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
        name, label, (unsigned long long)event.EventLabel.Id, timeUs,
        (unsigned long long)event.Value);
    break;
  }
}

void TraceFileSink::FlushBufferedEvents() noexcept
{
  for (const auto& event : m_BufferedEvents)
    WriteJsonEvent(event);
  m_BufferedEvents.clear();
}

}  // namespace gecko::runtime
