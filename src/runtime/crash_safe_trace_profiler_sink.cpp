#include "gecko/runtime/crash_safe_trace_profiler_sink.h"

#include <cstdio>
#if defined(_WIN32)
#include <corecrt_share.h>
#endif

#include "gecko/core/assert.h"

namespace gecko::runtime {

CrashSafeTraceProfilerSink::CrashSafeTraceProfilerSink(const char *path) {
  GECKO_ASSERT(path && "Trace file path cannot be null");

#if defined(_WIN32)
  m_File = _fsopen(path, "wb", _SH_DENYNO);
#else
  m_File = std::fopen(path, "wb");
#endif

  if (m_File) {
    // Write initial JSON structure
    std::fputs("{\"traceEvents\":[]}", m_File);
    std::fflush(m_File);
    m_First = true;
    m_Time0Ns = 0;
    m_EventCount.store(0, std::memory_order_relaxed);
  }
}

CrashSafeTraceProfilerSink::~CrashSafeTraceProfilerSink() {
  if (m_File) {
    EnsureValidJson();
    std::fclose(m_File);
  }
}

void CrashSafeTraceProfilerSink::Write(const ProfEvent &event) noexcept {
  if (!m_File)
    return;

  WriteEvent(event);

  // Periodically ensure valid JSON for crash safety
  size_t count = m_EventCount.fetch_add(1, std::memory_order_relaxed) + 1;
  if (count % FLUSH_INTERVAL == 0) {
    EnsureValidJson();
  }
}

void CrashSafeTraceProfilerSink::WriteBatch(const ProfEvent *events,
                                            size_t count) noexcept {
  if (!m_File || !events || count == 0)
    return;

  for (size_t i = 0; i < count; ++i) {
    WriteEvent(events[i]);
  }

  m_EventCount.fetch_add(count, std::memory_order_relaxed);
  EnsureValidJson(); // Always flush after batch
}

void CrashSafeTraceProfilerSink::WriteEvent(const ProfEvent &event) noexcept {
  if (m_Time0Ns == 0) {
    m_Time0Ns = event.TimestampNs;
  }

  // Seek back to overwrite the closing ]} and insert new event
  std::fseek(m_File, -2, SEEK_END); // Back up over ]}

  WriteSeparator();
  WriteJsonEvent(m_File, event, m_Time0Ns);
  std::fputs("]}", m_File); // Close JSON again
}

void CrashSafeTraceProfilerSink::WriteSeparator() noexcept {
  if (!m_First) {
    std::fputc(',', m_File);
  }
  m_First = false;
}

void CrashSafeTraceProfilerSink::EnsureValidJson() noexcept {
  if (m_File) {
    std::fflush(m_File);
  }
}

void CrashSafeTraceProfilerSink::Flush() noexcept { EnsureValidJson(); }

void CrashSafeTraceProfilerSink::WriteJsonEvent(std::FILE *file,
                                                const ProfEvent &event,
                                                u64 time0Ns) noexcept {
  const double timeUs = (double)(event.TimestampNs - time0Ns) / 1000.0;
  const char *name = event.Name ? event.Name : "Unknown";
  const char *label =
      event.EventLabel.Name ? event.EventLabel.Name : "label";

  switch (event.Kind) {
  case ProfEventKind::ZoneBegin:
    std::fprintf(file,
                 "{\"name\":\"%s\",\"cat\":\"%s "
                 "(%llu)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, label, (unsigned long long)event.EventLabel.Id, timeUs,
                 event.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    std::fprintf(file,
                 "{\"name\":\"%s\",\"cat\":\"%s "
                 "(%llu)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, label, (unsigned long long)event.EventLabel.Id, timeUs,
                 event.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    std::fprintf(file,
                 "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
                 "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
                 name, timeUs, event.ThreadId);
    break;
  case ProfEventKind::Counter:
    std::fprintf(
        file,
        "{\"name\":\"%s\",\"cat\":\"%s "
        "(%llu)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
        name, label, (unsigned long long)event.EventLabel.Id, timeUs,
        (unsigned long long)event.Value);
    break;
  }
}

} // namespace gecko::runtime
