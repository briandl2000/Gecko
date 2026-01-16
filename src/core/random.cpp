#include "gecko/core/random.h"

#include <cstring>
#include <random>

#include "gecko/core/assert.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"
#include "labels.h"

namespace gecko {

// Thread-local random state to avoid contention
struct ThreadRandomState {
  std::mt19937_64 generator;
  bool initialized = false;

  ThreadRandomState() {
    GECKO_PROF_SCOPE(core::labels::Random, "InitThreadRandom");
    // Seed with random device by default
    std::random_device rd;
    generator.seed(rd());
    initialized = true;
  }
};

static thread_local ThreadRandomState g_ThreadRandomState;

// Internal helper to get thread-local generator
static std::mt19937_64 &GetGenerator() noexcept {
  if (!g_ThreadRandomState.initialized) {
    g_ThreadRandomState = ThreadRandomState();
  }
  return g_ThreadRandomState.generator;
}

u32 RandomU32(u32 min, u32 max) noexcept {
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");

  if (min == max)
    return min;

  std::uniform_int_distribution<u32> dist(min, max);
  return dist(GetGenerator());
}

u64 RandomU64(u64 min, u64 max) noexcept {
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");

  if (min == max)
    return min;

  std::uniform_int_distribution<u64> dist(min, max);
  return dist(GetGenerator());
}

i32 RandomI32(i32 min, i32 max) noexcept {
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");

  if (min == max)
    return min;

  std::uniform_int_distribution<i32> dist(min, max);
  return dist(GetGenerator());
}

i64 RandomI64(i64 min, i64 max) noexcept {
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");

  if (min == max)
    return min;

  std::uniform_int_distribution<i64> dist(min, max);
  return dist(GetGenerator());
}

float RandomFloat(float min, float max) noexcept {
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  GECKO_ASSERT(std::isfinite(min) && std::isfinite(max) &&
               "Random range values must be finite");

  if (min == max)
    return min;

  std::uniform_real_distribution<float> dist(min, max);
  return dist(GetGenerator());
}

double RandomDouble(double min, double max) noexcept {
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  GECKO_ASSERT(std::isfinite(min) && std::isfinite(max) &&
               "Random range values must be finite");

  if (min == max)
    return min;

  std::uniform_real_distribution<double> dist(min, max);
  return dist(GetGenerator());
}

bool RandomBool() noexcept {
  std::bernoulli_distribution dist(0.5);
  return dist(GetGenerator());
}

void RandomBytes(void *buffer, size_t size) noexcept {
  GECKO_ASSERT(buffer && "Buffer cannot be null");

  if (size == 0)
    return;

  auto *bytes = static_cast<u8 *>(buffer);
  std::uniform_int_distribution<int> dist(0, 255);
  auto &gen = GetGenerator();

  for (size_t i = 0; i < size; ++i) {
    bytes[i] = static_cast<u8>(dist(gen));
  }
}

void SeedRandom(u64 seed) noexcept { GetGenerator().seed(seed); }

} // namespace gecko
