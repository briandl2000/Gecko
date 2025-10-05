#include "gecko/runtime/console_log_sink.h"

#include <cstdio>

#include "gecko/core/assert.h"
#include "gecko/core/log.h"

namespace gecko::runtime {

void ConsoleLogSink::Write(const LogMessage &message) noexcept {
  GECKO_ASSERT(message.Text && "Log message text cannot be null");

  std::fprintf((message.Level >= LogLevel::Warn) ? stderr : stdout,
               "[%s][%s][t%u] %s\n", LevelName(message.Level),
               message.Cat.Name ? message.Cat.Name : "cat", message.ThreadId,
               message.Text ? message.Text : "");
}

} // namespace gecko::runtime
