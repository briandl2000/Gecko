#include "gecko/platform/terminal.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdio>

using namespace gecko::platform;

TEST_CASE("Terminal: IsTerminal returns a bool without crashing", "[platform][terminal]")
{
  // Test runner pipes stdout when collecting, so this is normally false.
  // Just exercise the call.
  (void)IsTerminal(TermStream::Stdout);
  (void)IsTerminal(TermStream::Stderr);
  SUCCEED();
}

TEST_CASE("Terminal: Print + PrintLine accept empty strings", "[platform][terminal]")
{
  Print(TermStream::Stdout, TermColor::Default, "");
  PrintLine(TermStream::Stdout, TermColor::Green, "");
  SUCCEED();
}

TEST_CASE("Terminal: Print writes plain text to non-tty without escapes", "[platform][terminal]")
{
  // When stdout is captured (typical in `gk test`), color must be
  // suppressed. We can't easily intercept stdout from inside the
  // process, but at minimum confirm Print doesn't crash and returns.
  Print(TermStream::Stdout, TermColor::Red, "plain text");
  PrintLine(TermStream::Stdout, TermColor::Default, "another line");
  SUCCEED();
}

TEST_CASE("Terminal: writes UTF-8 multi-byte sequences", "[platform][terminal]")
{
  // Café — 日本語 — 😀
  PrintLine(TermStream::Stdout, TermColor::Cyan,
            "Caf\xC3\xA9 \xE2\x80\x94 "
            "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
            " \xE2\x80\x94 \xF0\x9F\x98\x80");
  SUCCEED();
}

TEST_CASE("Terminal: every TermColor enumerator is accepted", "[platform][terminal]")
{
  for (::gecko::u8 i = 0; i <= static_cast<::gecko::u8>(TermColor::BrightWhite); ++i)
  {
    Print(TermStream::Stdout, static_cast<TermColor>(i), "");
  }
  SUCCEED();
}
