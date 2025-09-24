#pragma once

#include <cstdio>

#include "gecko/core/log.h"

namespace gecko::runtime {

  class FileSink final : public ILogSink
  {
  public:
    explicit FileSink(const char* path);
    ~FileSink();
    virtual void Write(const LogMessage& message) noexcept override;

  private:
    std::FILE* m_File { nullptr };
  };

}
