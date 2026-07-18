#pragma once

#include "gecko/core/api.h"
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

struct ILogger
{
  virtual ~ILogger() = default;
  virtual void LogFormatted(LogLevel level, Label label, const char* format,
                            Span<const FormatArg> arguments) noexcept = 0;
  virtual void SetLevel(LogLevel level) noexcept = 0;
  [[nodiscard]] virtual LogLevel GetLevel() const noexcept = 0;
  [[nodiscard]] virtual bool Initialize() noexcept = 0;
  virtual void Shutdown() noexcept = 0;

  void Log(LogLevel level, Label label, const char* format) noexcept
  {
    LogFormatted(level, label, format, {});
  }

  template <typename First, typename... Rest>
  void Log(LogLevel level, Label label, const char* format, const First& first, const Rest&... rest) noexcept
  {
    const FormatArg arguments[] {MakeFormatArg(first), MakeFormatArg(rest)...};
    LogFormatted(level, label, format, Span<const FormatArg> {arguments, 1U + sizeof...(rest)});
  }
};

[[nodiscard]] GECKO_API ILogger* GetLogger() noexcept;

struct NullLogger final : ILogger
{
  void LogFormatted(LogLevel, Label, const char*, Span<const FormatArg>) noexcept override
  {}
  void SetLevel(LogLevel) noexcept override
  {}
  [[nodiscard]] LogLevel GetLevel() const noexcept override
  {
    return LogLevel::Info;
  }
  [[nodiscard]] bool Initialize() noexcept override
  {
    return true;
  }
  void Shutdown() noexcept override
  {}
};

}  // namespace gecko

#ifndef GECKO_LOGGING
#define GECKO_LOGGING 1
#endif

#if GECKO_LOGGING
#define GECKO_LOG(level, label, ...) gecko::GetLogger()->Log((level), (label), __VA_ARGS__)
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
