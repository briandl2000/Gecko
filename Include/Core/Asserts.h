#pragma once
#include "Defines.h"
#include <stdexcept>

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

#define ASSERT(expr, message)                                                       \
{                                                                                   \
    if (expr) { /* Do nothing */                                                    \
    } else {                                                                        \
        Gecko::Logger::ReportAssertionFailure(#expr, message, __FILE__, __LINE__);  \
        debugBreak();                                                               \
        throw std::exception(message);                                              \
    }                                                                               \
}

#else
#define ASSERT(expr, message)      // Does nothing at all
#endif

