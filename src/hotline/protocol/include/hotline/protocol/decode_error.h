// Hotline wire protocol: decode error codes.
//
// Decode functions return std::expected<T, DecodeError> because wire input
// is untrusted and must never throw or read out of bounds. Encode functions
// are the opposite: callers hold only program data, so programmer errors
// (e.g. a field larger than the 16-bit wire limit) throw std::length_error.

#pragma once

namespace hotline::protocol {

enum class DecodeError {
  // Fewer bytes than the declared layout requires.
  truncated,
  // A complete, valid message followed by surplus bytes (the historical
  // format has no padding, so surplus bytes mean the message is corrupt).
  trailing_bytes,
  // An integer field whose data size is neither 1, 2, nor 4 bytes.
  // Historical behavior: silently read as 0 (UFieldData::GetInteger).
  // Modern behavior: explicit error — a deliberate hardening divergence
  // that no valid peer can trigger (encoders only emit 2 or 4 bytes).
  invalid_integer_field_size,
};

}  // namespace hotline::protocol
