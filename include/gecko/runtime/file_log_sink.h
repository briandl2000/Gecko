#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/platform_io.h"

namespace gecko::runtime {

class FileLogSink final : public ILogSink
{
public:
  explicit FileLogSink(const char* path);
  ~FileLogSink();
  virtual void Write(const LogMessage& message) noexcept override;

private:
  ::gecko::Unique<::gecko::platform::IFileWriter> m_Writer {};
};

}  // namespace gecko::runtime
