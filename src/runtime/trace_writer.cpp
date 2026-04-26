#include "gecko/runtime/trace_writer.h"

#include "gecko/core/assert.h"
#include "gecko/platform/platform_io.h"
#include "private/file_writer_format.h"

namespace gecko::runtime {

namespace {

using ::gecko::runtime::detail::WriteFmt;

inline void WriteSep(::gecko::platform::FileWriter* w, bool& first) noexcept
{
  if (!first)
    w->WriteString(",");
  first = false;
}

}  // namespace

TraceWriter::TraceWriter() = default;

TraceWriter::~TraceWriter()
{
  Close();
}

bool TraceWriter::Open(const char* path)
{
  GECKO_ASSERT(path && "Trace file path cannot be null");

  Close();
  m_Writer = ::gecko::platform::OpenWrite(
      path, ::gecko::platform::WriteMode::Truncate);
  if (!m_Writer)
    return false;
  m_Writer->WriteString("{\"traceEvents\":[");
  m_First = true;
  m_Time0Ns = 0;
  return true;
}

void TraceWriter::Close()
{
  if (!m_Writer)
    return;
  m_Writer->WriteString("]}\n");
  m_Writer.reset();
}

void TraceWriter::Write(const ProfEvent& ev)
{
  if (!m_Writer)
    return;
  if (m_Time0Ns == 0)
    m_Time0Ns = ev.TimestampNs;

  const double timeUs = (double)(ev.TimestampNs - m_Time0Ns) / 1000.0;
  const char* name = ev.Name ? ev.Name : "Z";
  const char* label = ev.EventLabel.Name ? ev.EventLabel.Name : "label";

  switch (ev.Kind)
  {
  case ProfEventKind::ZoneBegin:
    WriteSep(m_Writer.get(), m_First);
    WriteFmt(m_Writer.get(),
             "{\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)ev.EventLabel.Id, timeUs,
             ev.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    WriteSep(m_Writer.get(), m_First);
    WriteFmt(m_Writer.get(),
             "{\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)ev.EventLabel.Id, timeUs,
             ev.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    WriteSep(m_Writer.get(), m_First);
    WriteFmt(m_Writer.get(),
             "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
             "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, timeUs, ev.ThreadId);
    break;
  case ProfEventKind::Counter:
    WriteSep(m_Writer.get(), m_First);
    WriteFmt(
        m_Writer.get(),
        "{\"name\":\"%s\",\"cat\":\"%s "
        "(%llu)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
        name, label, (unsigned long long)ev.EventLabel.Id, timeUs,
        (unsigned long long)ev.Value);
    break;
  }
}

}  // namespace gecko::runtime
