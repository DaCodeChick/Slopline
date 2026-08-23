// AppWarrior testing component.
//
// A tiny, dependency-free unit-test facility that ships with the framework,
// so every AppWarrior-based application (Hotline client/server/tracker,
// AppWarrior's own modules) shares one test vocabulary without pulling in a
// heavyweight third-party test framework. It is intentionally minimal — a
// test-case registry, assertion macros, byte-comparison helpers and a
// runner — and is deliberately replaceable: nothing outside it depends on
// its internals.
//
// Everything lives in namespace aw::test except the registration
// macros, which genuinely require preprocessing (a TEST_CASE must expand at
// its declaration site) and are therefore prefixed AW_ — the one case where
// a prefix is unavoidable, per AGENTS.md ("keep macros only when
// preprocessing itself is genuinely required").

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <exception>
#include <print>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aw::test {

struct Case {
  std::string_view name;
  void (*run)();
};

[[nodiscard]] inline auto registry() -> std::vector<Case>& {
  static std::vector<Case> cases;
  return cases;
}

struct Registrar {
  Registrar(std::string_view name, void (*run)()) { registry().push_back({name, run}); }
};

// Thrown by assertion failures to abort the current test case.
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

// Convenience: ASCII text as raw bytes (no terminator).
[[nodiscard]] inline auto bytes_from_ascii(std::string_view text) -> std::vector<std::byte> {
  std::vector<std::byte> out;
  out.reserve(text.size());
  for (const char character : text) {
    out.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
  }
  return out;
}

// Unwraps a std::expected in tests: fails the current case if the result
// holds an error. Keeps test call sites readable now that encode/decode
// APIs are expected-based.
template <typename T, typename E>
[[nodiscard]] auto unwrap(std::expected<T, E> result) -> T {
  if (!result.has_value()) {
    fail("expected value, got an error");
  }
  return *std::move(result);
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

// Runs every registered test case; returns 0 when all pass, 1 otherwise.
// Intended as the body of a test executable's main():
//
//   int main() { return aw::test::run_all_tests(); }
inline auto run_all_tests() -> int {
  std::size_t failed = 0;
  for (const Case& test : registry()) {
    try {
      test.run();
      std::println("PASS {}", test.name);
    } catch (const CheckFailed&) {
      std::println("FAIL {}", test.name);
      ++failed;
    } catch (const std::exception& error) {
      std::println("FAIL {} (exception: {})", test.name, error.what());
      ++failed;
    } catch (...) {
      std::println("FAIL {} (unknown exception)", test.name);
      ++failed;
    }
  }
  std::println("{} test case(s), {} failed", registry().size(), failed);
  return failed == 0 ? 0 : 1;
}

}  // namespace aw::test

// ---------------------------------------------------------------------------
// Registration macros (AW_-prefixed: macros cannot be namespaced)
// ---------------------------------------------------------------------------

#define AW_TEST_CAT_IMPL(a, b) a##b
#define AW_TEST_CAT(a, b) AW_TEST_CAT_IMPL(a, b)

#define AW_TEST_CASE(name)                                                        \
  static void AW_TEST_CAT(aw_test_fn_, __LINE__)();                               \
  static ::aw::test::Registrar AW_TEST_CAT(aw_test_reg_, __LINE__)(       \
      name, &AW_TEST_CAT(aw_test_fn_, __LINE__));                                 \
  static void AW_TEST_CAT(aw_test_fn_, __LINE__)()

#define AW_CHECK(expression)                                                      \
  do {                                                                            \
    if (!(expression)) {                                                          \
      ::aw::test::check_failed(#expression);                              \
    }                                                                             \
  } while (false)

#define AW_FAIL(message) ::aw::test::fail((message))

#define AW_REQUIRE_BYTES(actual, hex) ::aw::test::require_bytes((actual), (hex), "")
#define AW_REQUIRE_BYTES_MSG(actual, hex, context) \
  ::aw::test::require_bytes((actual), (hex), (context))
