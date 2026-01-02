#include "gecko/platform/platform_context.h"

#include <cstdio>

#include "categories.h"
#include "gecko/core/log.h"

namespace gecko::platform {

Unique<PlatformContext> PlatformContext::Create(const PlatformConfig &cfg) {
  GECKO_PROF_FUNC(categories::General);
  GECKO_INFO(categories::General, "PlatformContext::Create was called!\n");
  return CreateUnique<PlatformContext>();
}

} // namespace gecko::platform
