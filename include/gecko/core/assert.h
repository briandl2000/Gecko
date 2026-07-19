#pragma once

#include "gecko/api.h"
#include "gecko/core/types.h"

namespace gecko {

struct AssertInfo
{
  const char* Expression {nullptr};
  const char* Message {nullptr};
  const char* File {nullptr};
  const char* Function {nullptr};
  u32 Line {0};
};

[[noreturn]] GECKO_API void AssertFailure(const AssertInfo& info) noexcept;

namespace detail {

[[noreturn]] inline void DispatchAssert(const char* expression, const char* file, u32 line,
                                        const char* function) noexcept
{
  AssertFailure(AssertInfo {
      .Expression = expression,
      .File = file,
      .Function = function,
      .Line = line,
  });
}

[[noreturn]] inline void DispatchAssert(const char* expression, const char* file, u32 line, const char* function,
                                        const char* message) noexcept
{
  AssertFailure(AssertInfo {
      .Expression = expression,
      .Message = message,
      .File = file,
      .Function = function,
      .Line = line,
  });
}

}  // namespace detail

}  // namespace gecko

#if defined(NDEBUG)
#define GECKO_ASSERT(expression, ...) ((void)0)
#else
#define GECKO_ASSERT(expression, ...)                                                         \
  do                                                                                          \
  {                                                                                           \
    if (!(expression))                                                                        \
      gecko::detail::DispatchAssert(#expression, __FILE__, static_cast<gecko::u32>(__LINE__), \
                                    __func__ __VA_OPT__(, ) __VA_ARGS__);                     \
  } while (false)
#endif

#define GECKO_VERIFY(expression)                  \
  do                                              \
  {                                               \
    if (!(expression))                            \
      GECKO_ASSERT(false, "Verification failed"); \
  } while (false)
