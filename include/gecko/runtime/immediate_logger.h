#pragma once

#include "gecko/core/services/log.h"

namespace gecko::runtime {

class ImmediateLogger final : public ILogger
{
public:
  void LogFormatted(LogLevel level, Label label, const char* format,
                    Span<const FormatArg> arguments) noexcept override;
  void SetLevel(LogLevel level) noexcept override
  {
    m_Level = level;
  }
  [[nodiscard]] LogLevel GetLevel() const noexcept override
  {
    return m_Level;
  }
  [[nodiscard]] bool Initialize() noexcept override
  {
    return true;
  }
  void Shutdown() noexcept override
  {}

private:
  u32 m_Lock {0};
  LogLevel m_Level {LogLevel::Info};
};

}  // namespace gecko::runtime
