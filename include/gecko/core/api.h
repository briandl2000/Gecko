#pragma once

#include "gecko/core/platform.h"

#if defined(GECKO_PLATFORM_WINDOWS) && GECKO_BUILD_SHARED
#ifdef GECKO_BUILDING
#define GECKO_API __declspec(dllexport)
#else
#define GECKO_API __declspec(dllimport)
#endif
#else
#define GECKO_API
#endif
