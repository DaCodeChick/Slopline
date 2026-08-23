#include "support/test_support.h"

#include <cstddef>
#include <exception>
#include <print>

int main() {
  std::size_t failed = 0;
  for (const hotline_test::Case& test : hotline_test::registry()) {
    try {
      test.run();
      std::println("PASS {}", test.name);
    } catch (const hotline_test::CheckFailed&) {
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
  std::println("{} test case(s), {} failed", hotline_test::registry().size(), failed);
  return failed == 0 ? 0 : 1;
}
