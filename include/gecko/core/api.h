#pragma once

#if defined(_WIN32) && GECKO_BUILD_SHARED
  #ifdef GECKO_BUILDING
    #define GECKO_API __declspec(dllexport)
  #else
    #define GECKO_API __declspec(dllimport)
  #endif
#else
  #define GECKO_API
#endif
