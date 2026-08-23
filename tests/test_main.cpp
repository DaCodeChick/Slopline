// Test executable entry point: runs every case registered via
// AW_TEST_CASE in the linked translation units (AppWarrior testing
// component — aw::test::run_all_tests).

#include "appwarrior/testing.h"

int main() {
  return aw::test::run_all_tests();
}
