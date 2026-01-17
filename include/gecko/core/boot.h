#pragma once

#include "gecko/core/services.h"

#include <cstdlib>

#define GECKO_BOOT(servicesExpr)                 \
  do                                             \
  {                                              \
    if (!::gecko::InstallServices(servicesExpr)) \
    {                                            \
      ::std::exit(EXIT_FAILURE);                 \
    }                                            \
    ::gecko::ValidateServices(true);             \
  } while (0)

#define GECKO_SHUTDOWN()          \
  do                              \
  {                               \
    ::gecko::UninstallServices(); \
  } while (0)
