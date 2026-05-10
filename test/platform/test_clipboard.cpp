#include "gecko/platform/clipboard.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

// Clipboard tests are tagged [.clipboard] so they're hidden by default
// — they can race against another process holding the system selection
// (e.g. a real desktop session pasting into Catch's stdout). On X11 we
// own the selection ourselves between calls so a same-process Set→Get
// round-trip is reliable; on Win32 the OS-owned clipboard is global.
//
// Default unit-test runs leave the user's clipboard alone.

TEST_CASE("Clipboard: round-trip ASCII text", "[.clipboard][platform][clipboard]")
{
  using namespace gecko::platform;

  const ::std::string original = GetClipboardText();
  const ::std::string payload = "Gecko clipboard ASCII roundtrip";

  const bool ok = SetClipboardText(payload);
  if (!ok)
    SKIP("Clipboard not available on this platform/backend");

  const ::std::string read = GetClipboardText();
  REQUIRE(read == payload);

  // Best-effort restore.
  (void)SetClipboardText(original);
}

TEST_CASE("Clipboard: round-trip UTF-8 text", "[.clipboard][platform][clipboard]")
{
  using namespace gecko::platform;

  const ::std::string original = GetClipboardText();
  // 'Café — 日本語 — 😀'  (mixes 1/2/3/4-byte UTF-8 sequences)
  const ::std::string payload = "Caf\xC3\xA9 \xE2\x80\x94 "
                                "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
                                " \xE2\x80\x94 \xF0\x9F\x98\x80";

  const bool ok = SetClipboardText(payload);
  if (!ok)
    SKIP("Clipboard not available on this platform/backend");

  const ::std::string read = GetClipboardText();
  REQUIRE(read == payload);

  (void)SetClipboardText(original);
}

TEST_CASE("Clipboard: empty string is valid input", "[.clipboard][platform][clipboard]")
{
  using namespace gecko::platform;

  const ::std::string original = GetClipboardText();
  const bool ok = SetClipboardText("");
  if (!ok)
    SKIP("Clipboard not available on this platform/backend");

  const ::std::string read = GetClipboardText();
  REQUIRE(read.empty());

  (void)SetClipboardText(original);
}
