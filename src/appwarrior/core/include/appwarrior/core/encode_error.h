// AppWarrior Core: generic format-encode error codes.
//
// Encode functions return std::expected<T, EncodeError> for inputs that
// cannot be represented by the target format; they never throw. (The only
// exceptions remaining in the codebase are the test harness's assertion
// control flow and standard-library facilities.)

#pragma once

namespace aw {

enum class EncodeError {
  // An element (field, fork, entry, ...) is larger than the format allows.
  element_too_large,
  // The container holds more elements than the format allows.
  count_too_large,
  // A string exceeds the format's length limit.
  string_too_long,
};

}  // namespace aw
