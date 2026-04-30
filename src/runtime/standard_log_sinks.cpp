#include "gecko/runtime/standard_log_sinks.h"

#include "gecko/core/services.h"

namespace gecko::runtime {

StandardLogSinks::StandardLogSinks(const char* logFilePath,
                                   LogLevel level) noexcept
    : m_File(logFilePath)
{
  auto* logger = ::gecko::GetLogger();
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
