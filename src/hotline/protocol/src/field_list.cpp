#include "hotline/protocol/field_list.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "appwarrior/core/endian.h"

namespace hotline::protocol {

auto encode_field_list(const FieldList& list) -> std::vector<std::byte> {
  if (list.fields.size() > kMaxFieldCount) {
    throw std::length_error("field list has more than 65535 fields");
  }

  std::vector<std::byte> out;
  out.reserve(2 + 4 * list.fields.size());  // count + 4 header bytes per field (exact growth below)

  std::array<std::byte, 2> count_bytes{};
  aw::endian::write_u16be(static_cast<std::uint16_t>(list.fields.size()), count_bytes);
  out.insert(out.end(), count_bytes.begin(), count_bytes.end());

  for (const Field& field : list.fields) {
    if (field.data.size() > kMaxFieldDataSize) {
      throw std::length_error("field data larger than 65535 bytes");
    }

    std::array<std::byte, 2> id_bytes{};
    std::array<std::byte, 2> size_bytes{};
    aw::endian::write_u16be(static_cast<std::uint16_t>(field.id), id_bytes);
    aw::endian::write_u16be(static_cast<std::uint16_t>(field.data.size()), size_bytes);
    out.insert(out.end(), id_bytes.begin(), id_bytes.end());
    out.insert(out.end(), size_bytes.begin(), size_bytes.end());
    out.insert(out.end(), field.data.begin(), field.data.end());
  }

  return out;
}

auto decode_field_list(std::span<const std::byte> bytes)
    -> std::expected<FieldList, DecodeError> {
  FieldList list;

  if (bytes.size() < 2) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::uint16_t count = aw::endian::read_u16be(bytes.first<2>());
  bytes = bytes.subspan(2);
  list.fields.reserve(count);

  for (std::uint16_t i = 0; i < count; ++i) {
    if (bytes.size() < 4) {
      return std::unexpected(DecodeError::truncated);
    }
    const FieldId id = static_cast<FieldId>(aw::endian::read_u16be(bytes.first<2>()));
    const std::size_t size = aw::endian::read_u16be(bytes.subspan(2).first<2>());
    bytes = bytes.subspan(4);

    if (bytes.size() < size) {
      return std::unexpected(DecodeError::truncated);
    }

    Field field;
    field.id = id;
    const std::span<const std::byte> field_bytes = bytes.first(size);
    field.data.assign(field_bytes.begin(), field_bytes.end());
    bytes = bytes.subspan(size);

    list.fields.push_back(std::move(field));
  }

  if (!bytes.empty()) {
    return std::unexpected(DecodeError::trailing_bytes);
  }

  return list;
}

auto find_field(const FieldList& list, FieldId id) noexcept -> const Field* {
  for (const Field& field : list.fields) {
    if (field.id == id) {
      return &field;
    }
  }
  return nullptr;
}

auto find_field(FieldList& list, FieldId id) noexcept -> Field* {
  for (Field& field : list.fields) {
    if (field.id == id) {
      return &field;
    }
  }
  return nullptr;
}

auto field_data(const FieldList& list, FieldId id) noexcept -> std::span<const std::byte> {
  const Field* field = find_field(list, id);
  return field != nullptr ? std::span<const std::byte>(field->data) : std::span<const std::byte>{};
}

auto field_data(FieldList& list, FieldId id) noexcept -> std::span<std::byte> {
  Field* field = find_field(list, id);
  return field != nullptr ? std::span<std::byte>(field->data) : std::span<std::byte>{};
}

auto make_integer_field(FieldId id, std::int32_t value) -> Field {
  Field field;
  field.id = id;

  // UFieldData::AddInteger: 2 bytes iff (value & 0xFFFF0000) == 0, i.e.
  // the value fits the unsigned 16-bit range 0..65535 (negative values and
  // values above 65535 always take 4 bytes).
  if (value >= 0 && value <= 0xFFFF) {
    std::array<std::byte, 2> bytes{};
    aw::endian::write_u16be(static_cast<std::uint16_t>(value), bytes);
    field.data.assign(bytes.begin(), bytes.end());
  } else {
    std::array<std::byte, 4> bytes{};
    aw::endian::write_u32be(static_cast<std::uint32_t>(value), bytes);
    field.data.assign(bytes.begin(), bytes.end());
  }
  return field;
}

auto decode_integer_field(const Field& field)
    -> std::expected<std::int32_t, DecodeError> {
  const std::span<const std::byte> data = field.data;
  switch (data.size()) {
    case 1:
      // UFieldData::GetInteger reads a raw byte with no byte-order conversion.
      return static_cast<std::int32_t>(std::to_integer<std::uint8_t>(data[0]));
    case 2:
      // Zero-extended 16-bit value (the legacy code widened Uint16 -> Uint32).
      return static_cast<std::int32_t>(aw::endian::read_u16be(data.first<2>()));
    case 4:
      return static_cast<std::int32_t>(aw::endian::read_u32be(data.first<4>()));
    default:
      return std::unexpected(DecodeError::invalid_integer_field_size);
  }
}

auto make_string_field(FieldId id, std::string_view text) -> Field {
  Field field;
  field.id = id;
  field.data.reserve(text.size());
  for (const char character : text) {
    field.data.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
  }
  return field;
}

auto decode_string_field(const Field& field) -> std::string {
  std::string text;
  text.reserve(field.data.size());
  for (const std::byte byte : field.data) {
    text.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
  }
  return text;
}

}  // namespace hotline::protocol
