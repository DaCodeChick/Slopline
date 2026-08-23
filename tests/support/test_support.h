// Minimal dependency-free test harness.
//
// Deliberately tiny (registry + CHECK macros + a hex helper): the value is
// in the tests, and a zero-dependency harness keeps the build fully
// offline and auditable. It can be swapped for doctest later without
// touching the test bodies if that ever pays off.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <print>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace hotline_test {

struct Case {
  std::string_view name;
  void (*run)();
};

[[nodiscard]] inline auto& registry() {
  static std::vector<Case> cases;
  return cases;
}

struct Registrar {
  Registrar(std::string_view name, void (*run)()) { registry().push_back({name, run}); }
};

// Thrown by CHECK failures to abort the current test case.
struct CheckFailed final : std::exception {
  [[nodiscard]] auto what() const noexcept -> const char* override { return "check failed"; }
};

[[noreturn]] inline void check_failed(
    std::string_view expression,
    std::source_location location = std::source_location::current()) {
  std::println(stderr, "CHECK failed: {} ({}:{})", expression, location.file_name(),
               location.line());
  throw CheckFailed{};
}

[[noreturn]] inline void fail(std::string_view message,
                              std::source_location location = std::source_location::current()) {
  std::println(stderr, "FAIL: {} ({}:{})", message, location.file_name(), location.line());
  throw CheckFailed{};
}

[[nodiscard]] inline auto to_hex(std::span<const std::byte> bytes) -> std::string {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 3);
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    const unsigned value = std::to_integer<unsigned>(bytes[i]);
    if (i != 0) {
      out.push_back(' ');
    }
    out.push_back(kDigits[value >> 4]);
    out.push_back(kDigits[value & 0x0FU]);
  }
  return out;
}

// Parses "54 52 54 50 00 01 00 02" style hex (whitespace optional).
[[nodiscard]] inline auto bytes_from_hex(std::string_view hex) -> std::vector<std::byte> {
  const auto nibble = [](char digit) -> unsigned {
    if (digit >= '0' && digit <= '9') {
      return static_cast<unsigned>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
      return static_cast<unsigned>(digit - 'a' + 10);
    }
    if (digit >= 'A' && digit <= 'F') {
      return static_cast<unsigned>(digit - 'A' + 10);
    }
    throw std::invalid_argument("bad hex digit");
  };

  std::vector<std::byte> out;
  for (std::size_t i = 0; i < hex.size(); ++i) {
    const char first = hex[i];
    if (first == ' ' || first == '\t' || first == '\n') {
      continue;
    }
    if (i + 1 >= hex.size()) {
      throw std::invalid_argument("odd number of hex digits");
    }
    const unsigned hi = nibble(first);
    const unsigned lo = nibble(hex[i + 1]);
    ++i;
    out.push_back(static_cast<std::byte>((hi << 4) | lo));
  }
  return out;
}

// Compares actual bytes against the expected hex string, printing both on
// mismatch.
inline void require_bytes(std::span<const std::byte> actual, std::string_view expected_hex,
                          std::string_view context = "",
                          std::source_location location = std::source_location::current()) {
  const std::vector<std::byte> expected = bytes_from_hex(expected_hex);
  if (std::ranges::equal(expected, actual)) {
    return;
  }
  std::println(stderr, "byte mismatch {}({}:{})", context, location.file_name(), location.line());
  std::println(stderr, "  expected: {}", to_hex(expected));
  std::println(stderr, "  actual:   {}", to_hex(actual));
  throw CheckFailed{};
}

}  // namespace hotline_test

#define HOTLINE_TEST_CAT_IMPL(a, b) a##b
#define HOTLINE_TEST_CAT(a, b) HOTLINE_TEST_CAT_IMPL(a, b)

#define TEST_CASE(name)                                                              \
  static void HOTLINE_TEST_CAT(hotline_test_fn_, __LINE__)();                        \
  static ::hotline_test::Registrar HOTLINE_TEST_CAT(hotline_test_reg_, __LINE__)(    \
      name, &HOTLINE_TEST_CAT(hotline_test_fn_, __LINE__));                          \
  static void HOTLINE_TEST_CAT(hotline_test_fn_, __LINE__)()

#define CHECK(expression)                                            \
  do {                                                               \
    if (!(expression)) {                                             \
      ::hotline_test::check_failed(#expression);                     \
    }                                                                \
  } while (false)

#define REQUIRE_BYTES(actual, hex) ::hotline_test::require_bytes((actual), (hex), "")
#define REQUIRE_BYTES_MSG(actual, hex, context) \
  ::hotline_test::require_bytes((actual), (hex), (context))
