#include "gecko/runtime/file_sink.h"

#include <cstdio>
#if defined(_WIN32)
#include <corecrt_share.h>
#include <stdio.h>
#endif

#include "gecko/core/assert.h"

namespace gecko::runtime {

  FileSink::FileSink(const char* path)
  {
    GECKO_ASSERT(path && "File path cannot be null");
    
#if defined(_WIN32)
    m_File = _fsopen(path, "wb", _SH_DENYNO);
#else
    m_File = std::fopen(path, "wb");
#endif
  }
  
  FileSink::~FileSink()
  {
    if (m_File) std::fclose(m_File);
  }

  void FileSink::Write(const LogMessage& message) noexcept
  {
    if (!m_File) return;
    
    GECKO_ASSERT(message.Text && "Log message text cannot be null");
    
    std::fprintf(m_File,
                "[%s][%s][t%u] %s\n",
                LevelName(message.Level),
                message.Cat.Name ? message.Cat.Name : "cat",
                message.ThreadId,
                message.Text ? message.Text : "");
    std::fflush(m_File);
  }
}
