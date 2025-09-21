#include "gecko/platform/platform_context.h"

#include <cstdio>

#include "categories.h"

namespace gecko::platform 
{

  Unique<PlatformContext> PlatformContext::Create(const PlatformConfig &cfg)
  {
    GECKO_PROF_FUNC(categories::General);
    std::printf("PlatformContext::Create was called!");
    return CreateUnique<PlatformContext>();   
  }

}
