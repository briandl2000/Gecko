#pragma once

#include "gecko/core/services/log.h"
#include "gecko/core/sync.h"

namespace gecko::runtime {

class ImmediateLogger final
{
public:
  void LogFormatted(LogLevel level, Label label, const char* format, Span<const FormatArg> arguments) noexcept;
  void SetLevel(LogLevel level) noexcept
  {
    m_Level = level;
  }
  [[nodiscard]] LogLevel GetLevel() const noexcept
  {
    return m_Level;
  }
  [[nodiscard]] bool Initialize() noexcept
  {
    return true;
  }
  void Shutdown() noexcept
  {}

private:
  Mutex m_Mutex;
  LogLevel m_Level {LogLevel::Info};
};

}  // namespace gecko::runtime
