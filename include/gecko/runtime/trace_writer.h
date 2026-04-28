#pragma once

/// @file
/// `TraceWriter` -- low-level Chrome-trace JSON writer used by the
/// runtime trace sinks.

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/platform_io.h"

namespace gecko::runtime {

/// Internal helper that emits Chrome-trace-formatted `ProfEvent`s to
/// a `FileWriter`. Used by the trace sinks; not a service.
class TraceWriter
{
public:
  TraceWriter();
  ~TraceWriter();

  /// Open `path` and write the trace prologue. Returns `false` on failure.
  bool Open(const char* path);
  /// Write the trace epilogue and close the file.
  void Close();

  /// Append one `ProfEvent` as a JSON object.
  void Write(const ProfEvent& event);

private:
  ::gecko::Unique<::gecko::platform::FileWriter> m_Writer {};
  bool m_First {true};
  u64 m_Time0Ns {0};
};

}  // namespace gecko::runtime
