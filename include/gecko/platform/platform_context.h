#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/types.h"

namespace gecko::platform {

  struct PlatformConfig 
  {
  };

  class PlatformContext
  {
  public:
    GECKO_API virtual ~PlatformContext() = default;
 
    [[nodiscard]]
    GECKO_API static Unique<PlatformContext> Create(const PlatformConfig& cfg); 

  private:
    

  };

}
