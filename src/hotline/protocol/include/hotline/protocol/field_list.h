// Hotline wire protocol: field-list codec.
//
// Replaces the historical UFieldData buffer layout (AppWarrior
// Source/Data/UFieldData.cpp:2-11, :426-544, :554-600). A transaction body
// is a field list:
//
//   u16 count                       (big-endian)
//   count x { u16 id; u16 size; u8 data[size]; }
//
// There is NO padding between entries (the historical ALIGN_FIELDS switch
// is 0 in this codebase and in every peer it interoperates with).
//
// Preserved semantics:
//  * fields keep wire (insertion) order;
//  * duplicate IDs are legal and preserved verbatim (field IDs 112 and 114
//    are each used by two distinct fields — see constants.h);
//  * integer fields are encoded in 2 bytes when the value fits 0..65535,
//    4 bytes otherwise (UFieldData::AddInteger bit test); decoding accepts
//    1-, 2- or 4-byte data.
//
// Deliberate, documented divergences (hardening; no valid peer affected):
//  * decode rejects trailing bytes and oversized/unterminated entries
//    (the legacy reader walked the table and ignored garbage);
//  * decoding an integer field of size 3/5+/… is an explicit DecodeError
//    instead of a silent 0;
//  * encode permits zero-size fields, which the legacy emitter never
//    produced but legacy readers parse without trouble.
//
// Text fields are raw bytes on the wire: neither C strings (no NUL) nor
// Pascal strings (no length prefix) — UFieldData::AddCString writes strlen
// bytes and AddPString writes just the characters. Text encoding is the
// caller's concern (historical peers send MacRoman).

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "hotline/protocol/constants.h"
#include "hotline/protocol/decode_error.h"

namespace hotline::protocol {

struct Field {
  FieldId id = FieldId::ErrorText;
  std::vector<std::byte> data;
};

struct FieldList {
  std::vector<Field> fields;
};

// Encodes the list; throws std::length_error if it cannot be represented
// on the wire (more than 65535 fields, or any field larger than 65535
// bytes — the historical AddField limit).
[[nodiscard]] auto encode_field_list(const FieldList& list) -> std::vector<std::byte>;

// Decodes a complete field list; every malformed input shape maps to a
// DecodeError. Never throws, never reads out of bounds.
[[nodiscard]] auto decode_field_list(std::span<const std::byte> bytes)
    -> std::expected<FieldList, DecodeError>;

// First field with the given ID, or nullptr. With duplicate IDs this is
// the first in wire order (the legacy lookup table sorted by ID could
// return any of the duplicates; first-match is the documented successor).
[[nodiscard]] auto find_field(const FieldList& list, FieldId id) noexcept -> const Field*;
[[nodiscard]] auto find_field(FieldList& list, FieldId id) noexcept -> Field*;

// Data of the first field with the given ID; empty span if absent.
[[nodiscard]] auto field_data(const FieldList& list, FieldId id) noexcept -> std::span<const std::byte>;
[[nodiscard]] auto field_data(FieldList& list, FieldId id) noexcept -> std::span<std::byte>;

// --- typed helpers (mirror UFieldData::AddInteger/GetInteger/etc.) -------

// 2 bytes for 0..65535, 4 bytes otherwise (big-endian) — byte-identical to
// the historical AddInteger.
[[nodiscard]] auto make_integer_field(FieldId id, std::int32_t value) -> Field;

// 1-, 2- or 4-byte big-endian integer; anything else is
// DecodeError::invalid_integer_field_size.
[[nodiscard]] auto decode_integer_field(const Field& field)
    -> std::expected<std::int32_t, DecodeError>;

// Raw bytes of the text, no terminator (AddCString/AddPString wire form).
[[nodiscard]] auto make_string_field(FieldId id, std::string_view text) -> Field;

// Raw field bytes as a std::string, no terminator assumed or added.
[[nodiscard]] auto decode_string_field(const Field& field) -> std::string;

}  // namespace hotline::protocol
