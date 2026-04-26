#pragma once

#include "gecko/platform/platform_io.h"

#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <vector>

namespace gecko::runtime::detail {

// printf into a FileWriter without truncation. The common path uses a
// thread-local stack-sized buffer; rare overflows fall back to a
// thread-local growable heap buffer so profile/log hot paths do not
// allocate per call.
//
// `w` may be null (no-op). Format errors silently drop the record so
// the caller never has to check a return.
inline void WriteFmt(::gecko::platform::FileWriter* w, const char* fmt,
                     ...) noexcept
{
  if (!w)
    return;

  static constexpr ::std::size_t StackSize = 1024;
  thread_local char stack[StackSize];
  thread_local ::std::vector<char> heap;

  ::va_list ap;
  ::va_start(ap, fmt);
  ::va_list ap2;
  ::va_copy(ap2, ap);
  int n = ::std::vsnprintf(stack, StackSize, fmt, ap);
  ::va_end(ap);
  if (n < 0)
  {
    ::va_end(ap2);
    return;
  }

  if (static_cast<::std::size_t>(n) < StackSize)
  {
    ::va_end(ap2);
    w->WriteString(::std::string_view {stack, static_cast<::std::size_t>(n)});
    return;
  }

  const ::std::size_t needed = static_cast<::std::size_t>(n) + 1;
  if (heap.size() < needed)
    heap.resize(needed);
  int n2 = ::std::vsnprintf(heap.data(), heap.size(), fmt, ap2);
  ::va_end(ap2);
  if (n2 < 0)
    return;
  w->WriteString(
      ::std::string_view {heap.data(), static_cast<::std::size_t>(n2)});
}

}  // namespace gecko::runtime::detail
