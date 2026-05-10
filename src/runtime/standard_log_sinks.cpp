#include "gecko/runtime/standard_log_sinks.h"

#include "gecko/core/services.h"

namespace gecko::runtime {

StandardLogSinks::StandardLogSinks(const char* logFilePath, LogLevel level) noexcept : m_File(logFilePath)
{
  // Detect a *real* published logger via the registry. `GetLogger()`
  // would never return null (it falls back to a process-wide null
  // logger), so attaching against it would silently no-op while still
  // creating/truncating the log file. Looking up the service directly
  // gives us a precise "engine has booted with a real logger" check.
  auto* logger = ::gecko::GetModules()->Service<::gecko::ILogger>();
  if (!logger)
    return;
  m_Console.RegisterWith(logger);
  m_File.RegisterWith(logger);
  logger->SetLevel(level);
  m_Attached = true;
}

StandardLogSinks::~StandardLogSinks() noexcept
{
  if (!m_Attached)
    return;
  m_Console.Unregister();
  m_File.Unregister();
}

}  // namespace gecko::runtime
