#pragma once

#include "gecko/platform/platform_io.h"

#include <cstdarg>
#include <cstdio>
#include <string_view>

namespace gecko::runtime::detail {

// printf into a FileWriter without truncation up to a fixed stack-sized
// buffer. Records longer than the buffer are silently dropped.
//
// We deliberately avoid `thread_local` non-trivial destructors here:
// MinGW (Win32 GCC) crashes inside libstdc++ during thread / process
// teardown when a `thread_local std::vector<char>` is freed after some
// CRT state has already been torn down (the failure surfaces as a SIGSEGV
// inside `__new_allocator<char>::deallocate`). A 4 KiB stack-only buffer
// covers every chrome-trace and log-record format string we emit.
//
// `w` may be null (no-op). Format errors silently drop the record so
// the caller never has to check a return.
inline void WriteFmt(::gecko::platform::FileWriter* w, const char* fmt,
                     ...) noexcept
{
  if (!w)
    return;

  static constexpr ::std::size_t StackSize = 4096;
  char stack[StackSize];

  ::va_list ap;
  ::va_start(ap, fmt);
  int n = ::std::vsnprintf(stack, StackSize, fmt, ap);
  ::va_end(ap);
  if (n < 0)
    return;

  // Cap at StackSize-1 if vsnprintf indicates the formatted string was
  // truncated. We accept the truncation rather than allocating because
  // none of the engine's format strings should ever exceed StackSize.
  ::std::size_t len = static_cast<::std::size_t>(n);
  if (len >= StackSize)
    len = StackSize - 1;

  w->WriteString(::std::string_view {stack, len});
}

}  // namespace gecko::runtime::detail
