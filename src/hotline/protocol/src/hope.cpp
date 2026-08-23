#include "hotline/protocol/hope.h"

#include <algorithm>
#include <cstddef>
#include <ranges>

namespace hotline::protocol::auth::hope {

auto build_stage1_login() -> FieldList {
  FieldList list;
  list.fields.push_back(Field{FieldId::UserLogin, {std::byte{0}}});
  list.fields.push_back(Field{FieldId::UserPassword, {std::byte{0}}});
  list.fields.push_back(Field{FieldId::MacAlg, {kMacAlgList.begin(), kMacAlgList.end()}});
  list.fields.push_back(
      Field{FieldId::ClientCipherAlg, {kClientCipherAlgList.begin(), kClientCipherAlgList.end()}});
  return list;
}

auto parse_server_mac_algorithm(std::span<const std::byte> field)
    -> std::expected<MacAlgorithm, HopeError> {
  if (field.size() < 3) {
    return std::unexpected(HopeError::malformed);
  }
  const std::span<const std::byte> name = field.subspan(2);
  if (std::ranges::equal(name, kHmacSha1Name)) {
    return MacAlgorithm::hmac_sha1;
  }
  if (std::ranges::equal(name, kHmacMd5Name)) {
    return MacAlgorithm::hmac_md5;
  }
  return std::unexpected(HopeError::unsupported_mac_algorithm);
}

auto parse_server_cipher_algorithm(std::span<const std::byte> field)
    -> std::expected<CipherAlgorithm, HopeError> {
  if (field.size() < 3) {
    return std::unexpected(HopeError::malformed);
  }
  if (std::ranges::equal(field.subspan(2), kBlowfishName)) {
    return CipherAlgorithm::blowfish;
  }
  return std::unexpected(HopeError::unsupported_cipher_algorithm);
}

}  // namespace hotline::protocol::auth::hope
