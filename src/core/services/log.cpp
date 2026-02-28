#include "gecko/core/services/log.h"

namespace gecko {

void NullLogger::LogV(LogLevel /*level*/, Label /*label*/, const char* /*fmt*/,
                      va_list) noexcept
{}
void NullLogger::AddSink(ILogSink* /*sink*/) noexcept
{}
void NullLogger::RemoveSink(ILogSink* /*sink*/) noexcept
{}
void NullLogger::SetLevel(LogLevel /*level*/) noexcept
{}
LogLevel NullLogger::Level() const noexcept
{
  return LogLevel::Info;
}
void NullLogger::Flush() noexcept
{}
bool NullLogger::Init() noexcept
{
  return true;
}
void NullLogger::Shutdown() noexcept
{}

}  // namespace gecko
