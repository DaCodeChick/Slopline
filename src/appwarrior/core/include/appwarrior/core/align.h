// AppWarrior Core: power-of-two alignment rounding.
//
// Replaces the historical RoundUp2..64/RoundDown2..64 macros
// (legacy AppWarrior/Headers/typedefs.h:240-252) with typed constexpr
// functions. Alignment must be a non-zero power of two (precondition).
// (C++26 will provide std::align_up/std::align_down; the project targets
// C++23, and the historical macros must be replaced today.)

#pragma once

#include <concepts>
#include <cstdint>

namespace appwarrior::align {

template <std::unsigned_integral T>
[[nodiscard]] constexpr auto up(T value, T alignment) noexcept -> T {
  return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr auto down(T value, T alignment) noexcept -> T {
  return static_cast<T>(value & ~(alignment - 1));
}

}  // namespace appwarrior::align
