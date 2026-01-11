#include "gecko/runtime/trace_file_sink.h"

#include "gecko/core/assert.h"

namespace gecko::runtime {

TraceFileSink::TraceFileSink(const char *path) {
  GECKO_ASSERT(path && "Trace file path cannot be null");

#if defined(_WIN32)
  m_File = _fsopen(path, "w", _SH_DENYNO);
#else
  m_File = std::fopen(path, "w");
#endif

  if (m_File) {
    // Write initial JSON structure - we'll buffer events and write them all at
    // once
    std::fputs("{\"traceEvents\":[\n", m_File);
    m_First = true;
    m_Time0Ns = 0;
    m_BufferedEvents.reserve(1000); // Reserve space for better performance
  }
}

TraceFileSink::~TraceFileSink() {
  if (m_File) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    FlushBufferedEvents();
    std::fputs("]}\n", m_File); // Close JSON structure
    std::fclose(m_File);
  }
}

void TraceFileSink::Write(const ProfEvent &event) noexcept {
  if (!m_File)
    return;

  std::lock_guard<std::mutex> lock(m_Mutex);

  // Set time reference on first event
  if (m_Time0Ns == 0) {
    m_Time0Ns = event.TimestampNs;
  }

  // Buffer the event - we'll write in batches for better performance
  m_BufferedEvents.push_back(event);

  // Flush periodically to avoid using too much memory
  if (m_BufferedEvents.size() >= 100) {
    FlushBufferedEvents();
  }
}

void TraceFileSink::WriteBatch(const ProfEvent *events, size_t count) noexcept {
  if (!m_File || !events)
    return;

  for (size_t i = 0; i < count; ++i) {
    Write(events[i]);
  }
}

void TraceFileSink::Flush() noexcept {
  if (m_File) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    FlushBufferedEvents();
    std::fflush(m_File);
  }
}

void TraceFileSink::WriteJsonEvent(const ProfEvent &event) noexcept {
  const double timeUs = (double)(event.TimestampNs - m_Time0Ns) / 1000.0;
  const char *name = event.Name ? event.Name : "Unknown";
  const char *label = event.label.Name ? event.label.Name : "label";

  // Add comma separator if not first event
  if (!m_First) {
    std::fputs(",\n", m_File);
  }
  m_First = false;

  switch (event.Kind) {
  case ProfEventKind::ZoneBegin:
    std::fprintf(m_File,
                 "  {\"name\":\"%s\",\"cat\":\"%s "
                 "(%llu)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, label, (unsigned long long)event.label.Id, timeUs,
                 event.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    std::fprintf(m_File,
                 "  {\"name\":\"%s\",\"cat\":\"%s "
                 "(%llu)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, label, (unsigned long long)event.label.Id, timeUs,
                 event.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    std::fprintf(m_File,
                 "  "
                 "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
                 "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, timeUs, event.ThreadId);
    break;
  case ProfEventKind::Counter:
    std::fprintf(
        m_File,
        "  {\"name\":\"%s\",\"cat\":\"%s "
        "(%llu)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
        name, label, (unsigned long long)event.label.Id, timeUs,
        (unsigned long long)event.Value);
    break;
  }
}

void TraceFileSink::FlushBufferedEvents() noexcept {
  for (const auto &event : m_BufferedEvents) {
    WriteJsonEvent(event);
  }
  m_BufferedEvents.clear();
}

} // namespace gecko::runtime
