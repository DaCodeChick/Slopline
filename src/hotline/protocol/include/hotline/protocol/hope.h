// Hotline wire protocol: HOPE encrypted-login exchange codec.
//
// Reproduces the historical client-side HOPE flow exactly
// (HotlineTasks.cpp:1459-1650 — the feature is client-only in the
// reference tree; the server never emits a session key, so a compatible
// server-side reply builder is provided and marked as such):
//
// Stage 1 (client -> server): UserLogin = one zero byte, UserPassword =
// one zero byte, MacAlg = `00 02 09 "HMAC-SHA1"` (12 bytes — the
// historical buffer carried a second entry but only 12 bytes were sent,
// a quirk preserved here), C_CipherAlg = `00 01 08 "BLOWFISH"`.
//
// Stage 2 (server -> client): SessionKey (>= 32 bytes), MacAlg = 2-byte
// prefix + p-string algorithm name ("HMAC-SHA1" or "HMAC-MD5"),
// S_CipherAlg = 2-byte prefix + p-string "BLOWFISH". The client parses
// the names by comparing the field bytes AFTER the 2-byte prefix with the
// p-string (legacy GetName()).
//
// Stage 3 (client -> server): UserLogin = HMAC(login, sessionKey),
// UserPassword = HMAC(password, sessionKey) with the negotiated hash,
// C_CipherAlg = the server's cipher field echoed verbatim, Vers = 197.
// After sending, the client initializes HLCrypt with the PLAINTEXT
// password (the key schedule uses it — see key_schedule.h).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

#include "appwarrior/crypto/hmac.h"
#include "hotline/protocol/field_list.h"

namespace hotline::protocol::auth::hope {

// Algorithm names as the legacy p-strings (HLMD5.h / HLSha1.h /
// HLBlowfish.h GetName()): length byte + ASCII name.
namespace detail {
constexpr auto b(char c) noexcept -> std::byte {
  return static_cast<std::byte>(static_cast<unsigned char>(c));
}
}  // namespace detail

inline constexpr std::array<std::byte, 10> kHmacSha1Name{
    detail::b(9),  detail::b('H'), detail::b('M'), detail::b('A'), detail::b('C'),
    detail::b('-'), detail::b('S'), detail::b('H'), detail::b('A'), detail::b('1')};
inline constexpr std::array<std::byte, 9> kHmacMd5Name{
    detail::b(8),  detail::b('H'), detail::b('M'), detail::b('A'), detail::b('C'),
    detail::b('-'), detail::b('M'), detail::b('D'), detail::b('5')};
inline constexpr std::array<std::byte, 9> kBlowfishName{
    detail::b(8), detail::b('B'), detail::b('L'), detail::b('O'), detail::b('W'),
    detail::b('F'), detail::b('I'), detail::b('S'), detail::b('H')};

// Stage-1 request lists (verbatim, including the 12-byte quirk).
inline constexpr std::array<std::byte, 12> kMacAlgList{
    detail::b(0),  detail::b(2),  detail::b(9),  detail::b('H'), detail::b('M'),
    detail::b('A'), detail::b('C'), detail::b('-'), detail::b('S'), detail::b('H'),
    detail::b('A'), detail::b('1')};
inline constexpr std::array<std::byte, 11> kClientCipherAlgList{
    detail::b(0),  detail::b(1),  detail::b(8),  detail::b('B'), detail::b('L'),
    detail::b('O'), detail::b('W'), detail::b('F'), detail::b('I'), detail::b('S'),
    detail::b('H')};

// The client rejects session keys shorter than 32 bytes
// (HotlineTasks.cpp:1541-1546).
inline constexpr std::size_t kMinSessionKeySize = 32;

enum class MacAlgorithm : std::uint8_t { hmac_sha1, hmac_md5 };
enum class CipherAlgorithm : std::uint8_t { blowfish };

enum class HopeError {
  malformed,                  // field shorter than 2-byte prefix + name
  unsupported_mac_algorithm,
  unsupported_cipher_algorithm,
};

[[nodiscard]] auto build_stage1_login() -> FieldList;

// Legacy parse semantics: compare field bytes AFTER the 2-byte prefix
// with the p-string names (HotlineTasks.cpp:1559-1583).
[[nodiscard]] auto parse_server_mac_algorithm(std::span<const std::byte> field)
    -> std::expected<MacAlgorithm, HopeError>;
[[nodiscard]] auto parse_server_cipher_algorithm(std::span<const std::byte> field)
    -> std::expected<CipherAlgorithm, HopeError>;

// MacLogin / MacPassword (HotlineTasks.cpp:1611-1615).
template <aw::crypto::message_digest H>
[[nodiscard]] auto login_digests(std::span<const std::byte> login,
                                 std::span<const std::byte> password,
                                 std::span<const std::byte> session_key)
    -> std::pair<std::vector<std::byte>, std::vector<std::byte>> {
  return {aw::crypto::hmac<H>(login, session_key), aw::crypto::hmac<H>(password, session_key)};
}

// Stage-3 login: the digest fields, the server's cipher field echoed
// verbatim, and Vers = 197.
template <aw::crypto::message_digest H>
[[nodiscard]] auto build_stage2_login(std::span<const std::byte> login,
                                      std::span<const std::byte> password,
                                      std::span<const std::byte> session_key,
                                      std::span<const std::byte> server_cipher_field) -> FieldList {
  auto [mac_login, mac_password] = login_digests<H>(login, password, session_key);

  FieldList list;
  list.fields.push_back(Field{FieldId::UserLogin, std::move(mac_login)});
  list.fields.push_back(Field{FieldId::UserPassword, std::move(mac_password)});
  list.fields.push_back(
      Field{FieldId::ClientCipherAlg, {server_cipher_field.begin(), server_cipher_field.end()}});
  list.fields.push_back(make_integer_field(FieldId::Vers, 197));
  return list;
}

// Compatible server-side stage-2 reply: SessionKey, MacAlg =
// {0, 1, p-string name}, S_CipherAlg = {0, 1, p-string "BLOWFISH"}.
// (The reference tree's server never sent this; the layout mirrors the
// client's parser expectations.)
[[nodiscard]] inline auto build_server_stage2_reply(std::span<const std::byte> session_key,
                                                     MacAlgorithm algorithm) -> FieldList {
  const std::span<const std::byte> name = algorithm == MacAlgorithm::hmac_sha1
                                              ? std::span<const std::byte>(kHmacSha1Name)
                                              : std::span<const std::byte>(kHmacMd5Name);

  std::vector<std::byte> mac_field{std::byte{0}, std::byte{1}};
  mac_field.insert(mac_field.end(), name.begin(), name.end());

  std::vector<std::byte> cipher_field{std::byte{0}, std::byte{1}};
  cipher_field.insert(cipher_field.end(), kBlowfishName.begin(), kBlowfishName.end());

  FieldList list;
  list.fields.push_back(Field{FieldId::SessionKey, {session_key.begin(), session_key.end()}});
  list.fields.push_back(Field{FieldId::MacAlg, std::move(mac_field)});
  list.fields.push_back(Field{FieldId::ServerCipherAlg, std::move(cipher_field)});
  return list;
}

}  // namespace hotline::protocol::auth::hope
