#pragma once
#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"

/// @file
/// Combined memory-label-push + profiler scope macros.
///
/// The default `GECKO_SCOPE` family runs at `Detailed` level, so the
/// aggregator (`GetStats`/`WatchScope`) always observes the scope but
/// the ring/sink path is gated by `IProfiler::SetMinLevel` and
/// `SetDetailedSampleRate`.
///
/// | Macro | Level | Name source |
/// |---|---|---|
/// | `GECKO_SCOPE(label)`                  | Detailed | `__func__` |
/// | `GECKO_SCOPE_NAMED(label, name)`      | Detailed | custom |
/// | `GECKO_SCOPE_CAT(label, name, cat)`   | Detailed | custom + category id |
/// | `GECKO_SCOPE_NORMAL[_NAMED|_CAT]`     | Normal   | -- |
/// | `GECKO_SCOPE_ALWAYS[_NAMED|_CAT]`     | Always   | cannot be disabled |

#define GECKO_SCOPE(label) \
  GECKO_PUSH_LABEL(label); \
  GECKO_PROFILE(label)

#define GECKO_SCOPE_NAMED(label, name) \
  GECKO_PUSH_LABEL(label);             \
  GECKO_PROFILE_NAMED(label, name)

#define GECKO_SCOPE_CAT(label, name, cat) \
  GECKO_PUSH_LABEL(label);                \
  GECKO_PROFILE_CAT(label, name, cat)

#define GECKO_SCOPE_NORMAL(label) \
  GECKO_PUSH_LABEL(label);        \
  GECKO_PROFILE_NORMAL(label)

#define GECKO_SCOPE_NORMAL_NAMED(label, name) \
  GECKO_PUSH_LABEL(label);                    \
  GECKO_PROFILE_NORMAL_NAMED(label, name)

#define GECKO_SCOPE_NORMAL_CAT(label, name, cat) \
  GECKO_PUSH_LABEL(label);                       \
  GECKO_PROFILE_NORMAL_CAT(label, name, cat)

#define GECKO_SCOPE_ALWAYS(label) \
  GECKO_PUSH_LABEL(label);        \
  GECKO_PROFILE_ALWAYS(label)

#define GECKO_SCOPE_ALWAYS_NAMED(label, name) \
  GECKO_PUSH_LABEL(label);                    \
  GECKO_PROFILE_ALWAYS_NAMED(label, name)

#define GECKO_SCOPE_ALWAYS_CAT(label, name, cat) \
  GECKO_PUSH_LABEL(label);                       \
  GECKO_PROFILE_ALWAYS_CAT(label, name, cat)
