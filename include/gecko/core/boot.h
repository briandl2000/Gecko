#pragma once

#include "services.h"

#define GECKO_BOOT(servicesExpr)              \
  do {                                        \
    ::gecko::InstallServices(servicesExpr);  \
    ::gecko::ValidateServices(true);         \
  } while (0)

#define GECKO_SHUTDOWN()         \
  do {                           \
    ::gecko::UninstallServices(); \
  } while (0)
