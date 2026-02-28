#pragma once

#include "gecko/core/services/log.h"

#include <cstdio>

namespace gecko::runtime {

class FileLogSink final : public ILogSink
{
public:
  explicit FileLogSink(const char* path);
  ~FileLogSink();
  virtual void Write(const LogMessage& message) noexcept override;

private:
  std::FILE* m_File {nullptr};
};

}  // namespace gecko::runtime
