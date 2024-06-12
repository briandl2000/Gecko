#include "Logging/Logger.h"

#include "Logging/Asserts.h"
#include "Platform/Platform.h"

#include <stdarg.h>
#include <stdio.h>

namespace Gecko { namespace Logger
{

    bool Init()
    {
        return true;
    }

    void Shutdown()
    {

    }

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable: 4840)
#endif
    void LogOutput(eLogLevel level, std::string message, ...)
    {
        const char* level_strings[6] = { "[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[DEBUG]: ", "[TRACE]: " };
        message = level_strings[level] + message + "\n";

        char buffer[32000];

        va_list arg_ptr;
        va_start(arg_ptr, message);
        vsnprintf(buffer, 32000, message.c_str(), arg_ptr);
        va_end(arg_ptr);

        Platform::ConsoleWrite(buffer, level);
    }
#ifdef WIN32
#pragma warning(pop)
#endif

    void ReportAssertionFailure(const char* expression, const char* message, const char* file, i32 line)
    {
        LogOutput(LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d\n", expression, message, file, line);
    }

} }