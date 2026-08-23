#include "hotline/protocol/auth.h"

#include <cstddef>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::protocol::auth;
using namespace appwarrior::test;

AW_TEST_CASE("scramble: bitwise-NOT, self-inverse (legacy login obfuscation)") {
  const std::vector<std::byte> login = bytes_from_ascii("Hotline");
  AW_REQUIRE_BYTES_MSG(scramble(login), "b7 90 8b 93 96 91 9a", "scrambled 'Hotline'");

  // Applying it twice restores the original bytes — this is how the server
  // unscrambles the login before lowercasing it.
  const std::vector<std::byte> twice = scramble(scramble(login));
  AW_REQUIRE_BYTES_MSG(twice, "48 6f 74 6c 69 6e 65", "double scramble = identity");
}

AW_TEST_CASE("scramble: empty input is a no-op") {
  AW_CHECK(scramble(std::span<const std::byte>{}).empty());
}

AW_TEST_CASE("scramble: in-place overload matches the copy") {
  std::vector<std::byte> data = bytes_from_ascii("pw");
  const std::vector<std::byte> expected = scramble(std::span<const std::byte>(data));
  scramble(std::span<std::byte>(data));
  AW_CHECK(std::ranges::equal(data, expected));
}
