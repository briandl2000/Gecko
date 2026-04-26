#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/platform_io.h"

#include <mutex>
#include <vector>

namespace gecko::runtime {

class TraceFileSink final : public IProfilerSink
{
public:
  explicit TraceFileSink(const char* path);
  ~TraceFileSink();

  bool IsOpen() const noexcept
  {
    return m_Writer != nullptr;
  }

  virtual void Write(const ProfEvent& event) noexcept override;
  virtual void WriteBatch(const ProfEvent* events,
                          size_t count) noexcept override;
  virtual void Flush() noexcept override;

private:
  ::gecko::Unique<::gecko::platform::FileWriter> m_Writer {};
  bool m_First {true};
  u64 m_Time0Ns {0};
  std::vector<ProfEvent> m_BufferedEvents {};
  std::mutex m_Mutex {};

  void WriteJsonEvent(const ProfEvent& event) noexcept;
  void FlushBufferedEvents() noexcept;
};

}  // namespace gecko::runtime
