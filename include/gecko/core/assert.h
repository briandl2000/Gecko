#pragma once

#include <cassert>

#ifndef GECKO_ASSERT
#if defined(NDEBUG)
#define GECKO_ASSERT(x) ((void)0)
#else
#define GECKO_ASSERT(x) assert(x)
#endif
#endif

#ifndef GECKO_VERIFY
#define GECKO_VERIFY(x)                                                        \
  do {                                                                         \
    if (!(x)) {                                                                \
      GECKO_ASSERT(false && #x);                                               \
    }                                                                          \
  } while (0)
#endif
