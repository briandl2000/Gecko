#include "gecko/runtime/console_sink.h"

#include <cstdio>

#include "gecko/core/log.h"

namespace gecko::runtime {

  static const char* LevelName(LogLevel level) 
  {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Fatal: return "FATAL";
    default:
    }
    return "?";
  }
  
  void ConsoleSink::Write(const LogMessage& message) noexcept
  {
    std::fprintf((message.Level >= LogLevel::Warn)?stderr:stdout,
                "[%s][%s][t%u] %s\n",
                LevelName(message.Level),
                message.Category.Name ? message.Category.Name : "cat",
                message.ThreadId,
                message.Text ? message.Text : "");
  }

}
