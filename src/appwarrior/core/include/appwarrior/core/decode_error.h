// AppWarrior Core: generic format-decode error codes.
//
// Shared by every AppWarrior codec (and re-exported by Hotline protocol
// codecs): decode functions return std::expected<T, DecodeError> because
// untrusted input must never throw or read out of bounds. Encode
// functions likewise return std::expected<T, EncodeError> — no
// recoverable-error path in the production libraries throws
// (see encode_error.h).

#pragma once

namespace aw {

enum class DecodeError {
  // Fewer bytes than the declared layout requires.
  truncated,
  // A complete, valid message followed by surplus bytes (the historical
  // wire/disk formats carry no padding, so surplus bytes mean corruption).
  trailing_bytes,
  // An integer element whose data size is neither 1, 2, nor 4 bytes.
  // Historical behavior: silently read as 0 (UFieldData::GetInteger).
  // Modern behavior: explicit error — a deliberate hardening divergence
  // that no valid peer can trigger.
  invalid_integer_field_size,
  // A format tag ('FILP', 'RFLT', 'harc', 'HTRK', ...) that is not the
  // expected one.
  wrong_format_tag,
  // A version field the decoder does not support (legacy behavior:
  // fail with "version unknown").
  unsupported_version,
};

}  // namespace aw
