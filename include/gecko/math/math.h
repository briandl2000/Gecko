#pragma once

/// @file
/// Convenience aggregator for the Gecko math module.
///
/// The Gecko math module is header-only and lives in `::gecko::math`.
/// Including this header brings in every public type and free function
/// (vectors, AABBs, rectangles, matrices and rotors). Prefer including
/// the specific sub-header (e.g. `gecko/math/vector.h`) when you only
/// need a subset.

#include "gecko/math/aabb.h"
#include "gecko/math/matrix.h"
#include "gecko/math/quat.h"
#include "gecko/math/rect.h"
#include "gecko/math/vector.h"

namespace gecko::math {}
