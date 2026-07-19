#pragma once

#include "gecko/api.h"
#include "gecko/core/format.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

namespace gecko {

enum class LogLevel : u8
{
  Trace,
  Debug,
  Info,
  Warn,
  Error,
  Fatal,
};

GECKO_API void LogFormatted(LogLevel level, Label label, const char* format,
                            Span<const FormatArg> arguments = {}) noexcept;
GECKO_API void SetLogLevel(LogLevel level) noexcept;
[[nodiscard]] GECKO_API LogLevel GetLogLevel() noexcept;

inline void Log(LogLevel level, Label label, const char* format) noexcept
{
  LogFormatted(level, label, format);
}

template <typename First, typename... Rest>
void Log(LogLevel level, Label label, const char* format, const First& first, const Rest&... rest) noexcept
{
  const FormatArg arguments[] {MakeFormatArg(first), MakeFormatArg(rest)...};
  LogFormatted(level, label, format, Span<const FormatArg> {arguments, 1U + sizeof...(rest)});
}

}  // namespace gecko

#ifndef GECKO_LOGGING
#define GECKO_LOGGING 1
#endif

#if GECKO_LOGGING
#define GECKO_LOG(level, label, ...) gecko::Log((level), (label), __VA_ARGS__)
#define GECKO_TRACE(label, ...) GECKO_LOG(gecko::LogLevel::Trace, (label), __VA_ARGS__)
#define GECKO_DEBUG(label, ...) GECKO_LOG(gecko::LogLevel::Debug, (label), __VA_ARGS__)
#define GECKO_INFO(label, ...) GECKO_LOG(gecko::LogLevel::Info, (label), __VA_ARGS__)
#define GECKO_WARN(label, ...) GECKO_LOG(gecko::LogLevel::Warn, (label), __VA_ARGS__)
#define GECKO_ERROR(label, ...) GECKO_LOG(gecko::LogLevel::Error, (label), __VA_ARGS__)
#define GECKO_FATAL(label, ...) GECKO_LOG(gecko::LogLevel::Fatal, (label), __VA_ARGS__)
#else
#define GECKO_TRACE(label, ...)
#define GECKO_DEBUG(label, ...)
#define GECKO_INFO(label, ...)
#define GECKO_WARN(label, ...)
#define GECKO_ERROR(label, ...)
#define GECKO_FATAL(label, ...)
#endif
