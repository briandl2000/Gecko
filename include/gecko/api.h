#pragma once

// Public shared-library visibility. Public symbols that cross the Gecko
// boundary must use GECKO_API.
#if defined(GECKO_PLATFORM_WINDOWS) && GECKO_BUILD_SHARED
#if defined(GECKO_BUILDING)
#define GECKO_API __declspec(dllexport)
#else
#define GECKO_API __declspec(dllimport)
#endif
#else
#define GECKO_API
#endif
