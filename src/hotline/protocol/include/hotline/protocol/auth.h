// Hotline wire protocol: legacy login-field byte scrambling.
//
// The historical normal login path bitwise-NOTs every character byte of the
// login and password before sending (HotlineTasks.cpp:1494-1503) and the
// server NOTs them back (HotlineServTrans.cpp:1620-1623). The operation is
// self-inverse; it is obfuscation, not security — preserved verbatim for
// wire compatibility (AGENTS.md: do not silently alter protocol-required
// authentication).
//
// Documented server-side behavior the codec layer does NOT implement (it
// belongs to the future session/account layers):
//  * the login name is lowercased after unscrambling (UText::MakeLowercase,
//    encoding-aware) and '\r' characters are replaced with '-';
//  * the password is stored STILL SCRAMBLED in the user data file;
//  * the GetUser reply sends the scrambled login and masks the password as
//    the single byte 'x' (HotlineServTrans.cpp:3246-3263).
//
// Wire form: fields 105/106 carry the scrambled bytes with no terminator
// or length prefix (AddPString strips the Pascal length byte — see
// field_list.h).

#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace hotline::protocol::auth {

// Bitwise-NOT each byte, in place. Self-inverse.
constexpr void scramble(std::span<std::byte> bytes) noexcept {
  for (std::byte& byte : bytes) {
    byte = ~byte;
  }
}

// Convenience overload for borrowed data.
[[nodiscard]] inline auto scramble(std::span<const std::byte> bytes) -> std::vector<std::byte> {
  std::vector<std::byte> out(bytes.begin(), bytes.end());
  scramble(std::span<std::byte>(out));
  return out;
}

}  // namespace hotline::protocol::auth
