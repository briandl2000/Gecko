#include "gecko/runtime/console_log_sink.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/terminal.h"

#include <array>
#include <cstdio>
#include <string>

namespace gecko::runtime {

namespace {

[[nodiscard]] ::gecko::platform::TermColor LevelColor(LogLevel level) noexcept
{
  using ::gecko::platform::TermColor;
  switch (level)
  {
  case LogLevel::Trace:
    return TermColor::BrightBlack;
  case LogLevel::Debug:
    return TermColor::Cyan;
  case LogLevel::Info:
    return TermColor::Green;
  case LogLevel::Warn:
    return TermColor::Yellow;
  case LogLevel::Error:
    return TermColor::Red;
  case LogLevel::Fatal:
    return TermColor::BrightRed;
  }
  return TermColor::Default;
}

}  // namespace

void ConsoleLogSink::Write(const LogMessage& message) noexcept
{
  GECKO_ASSERT(message.Text && "Log message text cannot be null");

  const char* label = message.MessageLabel.Name ? message.MessageLabel.Name : "label";
  const char* text = message.Text ? message.Text : "";

  const auto stream =
      (message.Level >= LogLevel::Warn) ? ::gecko::platform::TermStream::Stderr : ::gecko::platform::TermStream::Stdout;

  // Format prefix + body into a small buffer, then push as one
  // colored payload. Using a stack scratch and falling back to
  // heap when the message overflows.
  ::std::array<char, 1024> stackBuf {};
  const int needed =
      ::std::snprintf(stackBuf.data(), stackBuf.size(), "[%s][%s] %s", LevelName(message.Level), label, text);

  ::std::string_view payload;
  ::std::string heap;
  if (needed > 0 && static_cast<::std::size_t>(needed) < stackBuf.size())
  {
    payload = ::std::string_view(stackBuf.data(), static_cast<::std::size_t>(needed));
  }
  else if (needed > 0)
  {
    // Reserve room for the trailing NUL that snprintf writes; the
    // payload string_view excludes it.
    const ::std::size_t len = static_cast<::std::size_t>(needed);
    heap.resize(len + 1);
    ::std::snprintf(heap.data(), heap.size(), "[%s][%s] %s", LevelName(message.Level), label, text);
    heap.resize(len);
    payload = heap;
  }

  ::gecko::platform::PrintLine(stream, LevelColor(message.Level), {payload.data(), payload.size()});
}

}  // namespace gecko::runtime
