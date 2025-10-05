#include "gecko/runtime/trace_writer.h"

#include <cstdio>
#if defined(_WIN32)
#include <corecrt_share.h>
#endif

#include "gecko/core/assert.h"

namespace gecko::runtime {

TraceWriter::TraceWriter() = default;

TraceWriter::~TraceWriter() { Close(); }

bool TraceWriter::Open(const char *path) {
  GECKO_ASSERT(path && "Trace file path cannot be null");

  Close();
#if defined(_WIN32)
  m_File = _fsopen(path, "wb", _SH_DENYNO);
#else
  m_File = std::fopen(path, "wb");
#endif
  if (!m_File)
    return false;
  std::fputs("{\"traceEvents\":[", m_File);
  m_First = true;
  m_Time0Ns = 0;
  return true;
}

void TraceWriter::Close() {
  if (!m_File)
    return;
  std::fputs("]}\n", m_File);
  std::fclose(m_File);
  m_File = nullptr;
}

static void WriteSep(FILE *file, bool &first) {
  if (!first)
    std::fputc(',', file);
  first = false;
}

void TraceWriter::Write(const ProfEvent &ev) {
  if (!m_File)
    return;
  if (m_Time0Ns == 0)
    m_Time0Ns = ev.TimestampNs;

  const double timeUs = (double)(ev.TimestampNs - m_Time0Ns) / 1000.0;
  const char *name = ev.Name ? ev.Name : "Z";

  switch (ev.Kind) {
  case ProfEventKind::ZoneBegin:
    WriteSep(m_File, m_First);
    std::fprintf(m_File,
                 "{\"name\":\"%s\",\"cat\":\"%s "
                 "(%u)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, ev.Cat.Name ? ev.Cat.Name : "Unknown", ev.Cat.Id, timeUs,
                 ev.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    WriteSep(m_File, m_First);
    std::fprintf(m_File,
                 "{\"name\":\"%s\",\"cat\":\"%s "
                 "(%u)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, ev.Cat.Name ? ev.Cat.Name : "Unknown", ev.Cat.Id, timeUs,
                 ev.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    WriteSep(m_File, m_First);
    std::fprintf(m_File,
                 "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
                 "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, timeUs, ev.ThreadId);
    break;
  case ProfEventKind::Counter:
    WriteSep(m_File, m_First);
    std::fprintf(
        m_File,
        "{\"name\":\"%s\",\"cat\":\"%s "
        "(%u)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
        name, ev.Cat.Name ? ev.Cat.Name : "Unknown", ev.Cat.Id, timeUs,
        (unsigned long long)ev.Value);
    break;
  }
}

} // namespace gecko::runtime
