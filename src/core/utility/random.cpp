#include "gecko/core/utility/random.h"

#include "gecko/core/assert.h"

#include <cstring>
#include <random>

namespace gecko {

struct ThreadRandomState
{
  ::std::mt19937_64 generator;
  ::std::uniform_real_distribution<f32> dist01_f32 {0.0f, 1.0f};
  ::std::uniform_real_distribution<f64> dist01_f64 {0.0, 1.0};
  ::std::bernoulli_distribution distBool {0.5};
  bool initialized = false;

  ThreadRandomState()
  {
    ::std::random_device rd;
    generator.seed(rd());
    initialized = true;
  }
};

static thread_local ThreadRandomState g_ThreadRandomState;

static ::std::mt19937_64& GetGenerator() noexcept
{
  if (!g_ThreadRandomState.initialized)
    g_ThreadRandomState = ThreadRandomState();
  return g_ThreadRandomState.generator;
}

u32 RandomU32(u32 min, u32 max) noexcept
{
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  if (min == max)
    return min;
  ::std::uniform_int_distribution<u32> dist(min, max);
  return dist(GetGenerator());
}

u64 RandomU64(u64 min, u64 max) noexcept
{
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  if (min == max)
    return min;
  ::std::uniform_int_distribution<u64> dist(min, max);
  return dist(GetGenerator());
}

i32 RandomI32(i32 min, i32 max) noexcept
{
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  if (min == max)
    return min;
  ::std::uniform_int_distribution<i32> dist(min, max);
  return dist(GetGenerator());
}

i64 RandomI64(i64 min, i64 max) noexcept
{
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  if (min == max)
    return min;
  ::std::uniform_int_distribution<i64> dist(min, max);
  return dist(GetGenerator());
}

f32 RandomF32(f32 min, f32 max) noexcept
{
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  GECKO_ASSERT(::std::isfinite(min) && ::std::isfinite(max));
  if (min == max)
    return min;
  f32 t = g_ThreadRandomState.dist01_f32(GetGenerator());
  return min + t * (max - min);
}

f64 RandomF64(f64 min, f64 max) noexcept
{
  GECKO_ASSERT(min <= max && "Random range: min must be <= max");
  GECKO_ASSERT(::std::isfinite(min) && ::std::isfinite(max));
  if (min == max)
    return min;
  f64 t = g_ThreadRandomState.dist01_f64(GetGenerator());
  return min + t * (max - min);
}

bool RandomBool() noexcept
{
  return g_ThreadRandomState.distBool(GetGenerator());
}

void RandomBytes(void* buffer, usize size) noexcept
{
  GECKO_ASSERT(buffer && "Buffer cannot be null");
  if (size == 0)
    return;

  auto* bytes = static_cast<u8*>(buffer);
  auto& gen = GetGenerator();

  while (size >= 8)
  {
    u64 val = gen();
    ::std::memcpy(bytes, &val, 8);
    bytes += 8;
    size -= 8;
  }

  if (size > 0)
  {
    u64 val = gen();
    ::std::memcpy(bytes, &val, size);
  }
}

void SeedRandom(u64 seed) noexcept
{
  GetGenerator().seed(seed);
}

}  // namespace gecko
