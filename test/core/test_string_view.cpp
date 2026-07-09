#include "gecko/core/string_view.h"

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using namespace gecko;

TEST_CASE("StringView default construction", "[core][string_view]")
{
  constexpr StringView view {};

  STATIC_REQUIRE(::std::is_trivially_copyable_v<StringView>);
  REQUIRE(view.Data() == nullptr);
  REQUIRE(view.Size() == 0);
  REQUIRE(view.Empty());
}

TEST_CASE("StringView views explicit pointer and size", "[core][string_view]")
{
  const char text[] = {'g', 'e', 'c', 'k', 'o', '\0', 'x'};
  StringView view {text, 7};

  REQUIRE(view.Data() == text);
  REQUIRE(view.Size() == 7);
  REQUIRE_FALSE(view.Empty());
  REQUIRE(view[0] == 'g');
  REQUIRE(view[5] == '\0');
  REQUIRE(*(view.end() - 1) == 'x');
}

TEST_CASE("StringView literal constructor drops terminator", "[core][string_view]")
{
  constexpr StringView view {"gecko"};

  STATIC_REQUIRE(view.Size() == 5);
  REQUIRE(view.Data()[view.Size()] == '\0');
}

TEST_CASE("StringView compares by bytes", "[core][string_view]")
{
  constexpr StringView a {"same"};
  constexpr StringView b {"same"};
  constexpr StringView c {"different"};

  STATIC_REQUIRE(a == b);
  STATIC_REQUIRE(a != c);
  REQUIRE(a == b);
  REQUIRE(a != c);
}
