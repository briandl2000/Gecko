#pragma once
#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"

#define GECKO_SCOPE_NAMED(label, name) \
  GECKO_PUSH_LABEL(label);             \
  GECKO_PROF_SCOPE_NAMED(label, name)

#define GECKO_SCOPE(label) \
  GECKO_PUSH_LABEL(label); \
  GECKO_PROF_SCOPE(label)

#define GECKO_FUNC(label)  \
  GECKO_PUSH_LABEL(label); \
  GECKO_PROF_FUNC(label)

#define GECKO_SCOPE_NAMED_DETAILED(label, name) \
  GECKO_PUSH_LABEL(label);                      \
  GECKO_PROF_SCOPE_NAMED_DETAILED(label, name)

#define GECKO_SCOPE_DETAILED(label) \
  GECKO_PUSH_LABEL(label);          \
  GECKO_PROF_SCOPE_DETAILED(label)

#define GECKO_FUNC_DETAILED(label) \
  GECKO_PUSH_LABEL(label);         \
  GECKO_PROF_FUNC_DETAILED(label)

#define GECKO_SCOPE_NAMED_MARK(label, name) \
  GECKO_PUSH_LABEL(label);                  \
  GECKO_PROF_SCOPE_NAMED_MARK(label, name)

#define GECKO_SCOPE_MARK(label) \
  GECKO_PUSH_LABEL(label);      \
  GECKO_PROF_SCOPE_MARK(label)

#define GECKO_FUNC_MARK(label) \
  GECKO_PUSH_LABEL(label);     \
  GECKO_PROF_FUNC_MARK(label)
