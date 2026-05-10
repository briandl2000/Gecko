#pragma once

#include "file_writer_format.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/platform_io.h"

namespace gecko::runtime::detail {

// Single-event chrome://tracing JSON formatter shared by the crash-safe and
// async trace sinks. `time0Ns` is the trace's start-of-time (subtract from
// each event's TimestampNs to get a relative microsecond stamp).
inline void WriteChromeTraceEvent(::gecko::platform::FileWriter* w, const ProfEvent& event, u64 time0Ns) noexcept
{
  if (!w)
    return;

  const double timeUs = (double)(event.TimestampNs - time0Ns) / 1000.0;
  const char* name = event.Name ? event.Name : "Unknown";
  const char* label = event.EventLabel.Name ? event.EventLabel.Name : "label";

  switch (event.Kind)
  {
  case ProfEventKind::ZoneBegin:
    WriteFmt(w,
             "{\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"B\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs, event.ThreadId);
    break;
  case ProfEventKind::ZoneEnd:
    WriteFmt(w,
             "{\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"E\",\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs, event.ThreadId);
    break;
  case ProfEventKind::FrameMark:
    WriteFmt(w,
             "{\"name\":\"%s\",\"cat\":\"frame\",\"ph\":\"i\",\"s\":\"t\","
             "\"ts\":%.3f,\"pid\":1,\"tid\":%u}",
             name, timeUs, event.ThreadId);
    break;
  case ProfEventKind::Counter:
    WriteFmt(w,
             "{\"name\":\"%s\",\"cat\":\"%s "
             "(%llu)\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":1,\"args\":{\"v\":%llu}}",
             name, label, (unsigned long long)event.EventLabel.Id, timeUs, (unsigned long long)event.Value);
    break;
  }
}

// Emit a chrome-trace `thread_name` metadata record. Should be written once
// per thread on first sight by sinks that care.
inline void WriteChromeTraceThreadName(::gecko::platform::FileWriter* w, u32 threadId, const char* name) noexcept
{
  if (!w || !name)
    return;
  WriteFmt(w,
           "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":1,\"tid\":%u,"
           "\"args\":{\"name\":\"%s\"}}",
           threadId, name);
}

}  // namespace gecko::runtime::detail
