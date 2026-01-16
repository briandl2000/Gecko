#pragma once

#include "gecko/core/services/profiler.h"

#include <cstdio>

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
  std::FILE* m_File {nullptr};
  bool m_First {true};
  u64 m_Time0Ns {0};
};

}  // namespace gecko::runtime
