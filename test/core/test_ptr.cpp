#include "gecko/core/ptr.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

struct TestObj
{
  int Value;
  explicit TestObj(int v) : Value(v)
  {}
};

TEST_CASE("CreateUnique creates a valid unique_ptr", "[core][ptr]")
{
  auto ptr = CreateUnique<TestObj>(42);
  REQUIRE(ptr != nullptr);
  REQUIRE(ptr->Value == 42);
}

TEST_CASE("CreateShared creates a valid shared_ptr", "[core][ptr]")
{
  auto ptr = CreateShared<TestObj>(99);
  REQUIRE(ptr != nullptr);
  REQUIRE(ptr->Value == 99);
  REQUIRE(ptr.use_count() == 1);
}

TEST_CASE("CreateUniqueFromRaw takes ownership", "[core][ptr]")
{
  auto* raw = new TestObj(7);
  auto ptr = CreateUniqueFromRaw(raw);
  REQUIRE(ptr != nullptr);
  REQUIRE(ptr->Value == 7);
}

TEST_CASE("CreateSharedFromRaw takes ownership", "[core][ptr]")
{
  auto* raw = new TestObj(13);
  auto ptr = CreateSharedFromRaw(raw);
  REQUIRE(ptr != nullptr);
  REQUIRE(ptr->Value == 13);
  REQUIRE(ptr.use_count() == 1);
}

TEST_CASE("CreateWeakFromShared creates a weak reference", "[core][ptr]")
{
  auto shared = CreateShared<TestObj>(21);
  auto weak = CreateWeakFromShared(shared);
  REQUIRE_FALSE(weak.expired());

  auto locked = weak.lock();
  REQUIRE(locked != nullptr);
  REQUIRE(locked->Value == 21);
}

TEST_CASE("Weak pointer expires when shared is released", "[core][ptr]")
{
  Weak<TestObj> weak;
  {
    auto shared = CreateShared<TestObj>(55);
    weak = CreateWeakFromShared(shared);
    REQUIRE_FALSE(weak.expired());
  }
  REQUIRE(weak.expired());
}
