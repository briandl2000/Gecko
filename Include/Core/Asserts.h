#pragma once
#include "Defines.h"

// Disable assertions by commenting out the below line.
#define ASSERTIONS_ENABLED

#ifdef ASSERTIONS_ENABLED
#if _MSC_VER
#include <intrin.h>
#define debugBreak() __debugbreak()
#else
#define debugBreak() __builtin_trap()
#endif

namespace Gecko { namespace Logger {
    void ReportAssertionFailure(const char* expression, const char* message, const char* file, i32 line);
} }

#define ASSERT(expr)                                                \
    {                                                                \
        if (expr) {                                                  \
        } else {                                                     \
            Gecko::Logger::ReportAssertionFailure(#expr, "", __FILE__, __LINE__); \
            debugBreak();                                            \
        }                                                            \
    }

#define ASSERT_MSG(expr, message)                                        \
    {                                                                     \
        if (expr) {                                                       \
        } else {                                                          \
            Gecko::Logger::ReportAssertionFailure(#expr, message, __FILE__, __LINE__); \
            debugBreak();                                                 \
        }                                                                 \
    }

#else
#define ASSERT(expr)               // Does nothing at all
#define ASSERT_MSG(expr, message)  // Does nothing at all
#define ASSERT_DEBUG(expr)         // Does nothing at all
#endif