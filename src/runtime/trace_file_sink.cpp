#include "gecko/runtime/trace_file_sink.h"

#include "gecko/core/assert.h"
#include "gecko/platform/platform_io.h"
#include "private/file_writer_format.h"

#include <mutex>
#include <new>
#include <vector>

namespace gecko::runtime {

namespace {

using ::gecko::runtime::detail::WriteFmt;

}  // namespace

struct TraceFileSink::Impl
{
  ::gecko::Unique<::gecko::platform::FileWriter> Writer {};
  bool First {true};
  u64 Time0Ns {0};
  std::vector<ProfEvent> BufferedEvents {};
  std::mutex Mutex {};
};

TraceFileSink::TraceFileSink(const char* path)
{
  GECKO_ASSERT(path && "Trace file path cannot be null");

  m_Impl.reset(new (::std::nothrow) Impl());
  if (!m_Impl)
    return;

  m_Impl->Writer = ::gecko::platform::OpenWrite(path, ::gecko::platform::WriteMode::Truncate);

  if (m_Impl->Writer)
  {
    m_Impl->Writer->WriteString("{\"traceEvents\":[\n");
    m_Impl->First = true;
    m_Impl->Time0Ns = 0;
    m_Impl->BufferedEvents.reserve(1000);
  }
}

TraceFileSink::~TraceFileSink()
{
  Unregister();
  if (m_Impl && m_Impl->Writer)
  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    FlushBufferedEvents();
    m_Impl->Writer->WriteString("]}\n");
    m_Impl->Writer.reset();
  }
}

bool TraceFileSink::IsOpen() const noexcept
{
  return m_Impl && m_Impl->Writer;
}

void TraceFileSink::Write(const ProfEvent& event) noexcept
{
  if (!m_Impl || !m_Impl->Writer)
    return;

  std::lock_guard<std::mutex> lock(m_Impl->Mutex);

  if (m_Impl->Time0Ns == 0)
    m_Impl->Time0Ns = event.TimestampNs;

  m_Impl->BufferedEvents.push_back(event);

  if (m_Impl->BufferedEvents.size() >= 100)
    FlushBufferedEvents();
}

void TraceFileSink::WriteBatch(::gecko::Span<const ProfEvent> events) noexcept
{
  if (!m_Impl || !m_Impl->Writer)
    return;

  for (const ProfEvent& e : events)
    Write(e);
}

void TraceFileSink::Flush() noexcept
{
  if (m_Impl && m_Impl->Writer)
  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    FlushBufferedEvents();
    m_Impl->Writer->Flush();
  }
}

void TraceFileSink::WriteJsonEvent(const ProfEvent& event) noexcept
{
  const double timeUs = (double)(event.TimestampNs - m_Impl->Time0Ns) / 1000.0;
  const char* name = event.Name ? event.Name : "Unknown";
  const char* label = event.EventLabel.Name ? event.EventLabel.Name : "label";

  if (!m_Impl->First)
    m_Impl->Writer->WriteString(",\n");
  m_Impl->First = false;

  switch (event.Kind)
  {
  case ProfEventKind::ZoneBegin:
    WriteFmt(m_Impl->Writer.get(),
             "  {\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs, event.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    WriteFmt(m_Impl->Writer.get(),
             "  {\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs, event.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    WriteFmt(m_Impl->Writer.get(),
             "  "
             "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
             "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, timeUs, event.ThreadId);
    break;
  case ProfEventKind::Counter:
    WriteFmt(m_Impl->Writer.get(),
             "  {\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs, (unsigned long long)event.Value);
    break;
  }
}

void TraceFileSink::FlushBufferedEvents() noexcept
{
  if (!m_Impl)
    return;

  for (const auto& event : m_Impl->BufferedEvents)
    WriteJsonEvent(event);
  m_Impl->BufferedEvents.clear();
}

}  // namespace gecko::runtime
