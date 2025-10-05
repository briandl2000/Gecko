#pragma once

#include <cstdio>
#include <mutex>
#include <vector>

#include "gecko/core/profiler.h"

namespace gecko::runtime {

class TraceFileSink final : public IProfilerSink {
public:
  explicit TraceFileSink(const char *path);
  ~TraceFileSink();

  bool IsOpen() const noexcept { return m_File != nullptr; }

  virtual void Write(const ProfEvent &event) noexcept override;
  virtual void WriteBatch(const ProfEvent *events,
                          size_t count) noexcept override;
  virtual void Flush() noexcept override;

private:
  std::FILE *m_File{nullptr};
  bool m_First{true};
  u64 m_Time0Ns{0};
  std::vector<ProfEvent> m_BufferedEvents{};
  std::mutex m_Mutex{}; // Thread safety for multi-threaded access

  void WriteJsonEvent(const ProfEvent &event) noexcept;
  void FlushBufferedEvents() noexcept;
};

} // namespace gecko::runtime
