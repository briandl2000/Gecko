#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/platform_io.h"

namespace gecko::runtime {

class TraceWriter
{
public:
  TraceWriter();
  ~TraceWriter();

  bool Open(const char* path);
  void Close();

  void Write(const ProfEvent& event);

private:
  ::gecko::Unique<::gecko::platform::FileWriter> m_Writer {};
  bool m_First {true};
  u64 m_Time0Ns {0};
};

}  // namespace gecko::runtime
