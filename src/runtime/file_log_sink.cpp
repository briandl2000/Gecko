#include "gecko/runtime/file_log_sink.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/platform/platform_io.h"
#include "private/labels.h"

#include <cstdio>

namespace gecko::runtime {

FileLogSink::FileLogSink(const char* path)
{
  GECKO_ASSERT(path && "File path cannot be null");
  m_Writer = ::gecko::platform::OpenWrite(path, ::gecko::platform::WriteMode::Append);
}

FileLogSink::~FileLogSink() = default;

void FileLogSink::Write(const LogMessage& message) noexcept
{
  if (!m_Writer)
    return;

  GECKO_PROFILE_NAMED(labels::Logger, "FileLogSink::Write");

  GECKO_ASSERT(message.Text && "Log message text cannot be null");

  const char* label = message.MessageLabel.Name ? message.MessageLabel.Name : "label";

  char buf[2048];
  int n = std::snprintf(buf, sizeof(buf), "[%s][%s][t%u] %s\n", LevelName(message.Level), label, message.ThreadId,
                        message.Text ? message.Text : "");
  if (n <= 0)
    return;
  ::std::size_t len = (n >= static_cast<int>(sizeof(buf))) ? sizeof(buf) - 1 : static_cast<::std::size_t>(n);
  m_Writer->WriteString(::std::string_view {buf, len});
  {
    GECKO_PROFILE_NAMED(labels::Logger, "FileLogSink::Flush");
    m_Writer->Flush();
  }
}
}  // namespace gecko::runtime
