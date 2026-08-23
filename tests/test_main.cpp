// Test executable entry point: runs every case registered via
// AW_TEST_CASE in the linked translation units (AppWarrior testing
// component — appwarrior::test::run_all_tests).

#include "appwarrior/testing.h"

int main() {
  return appwarrior::test::run_all_tests();
}
