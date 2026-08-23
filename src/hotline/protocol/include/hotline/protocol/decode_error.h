// Hotline wire protocol: decode error codes.
//
// The protocol re-exports the framework's generic decode errors: decoding
// is std::expected<T, DecodeError>, never throwing, never out of bounds
// (see appwarrior/core/decode_error.h).

#pragma once

#include "appwarrior/core/decode_error.h"

namespace hotline::protocol {

using DecodeError = aw::DecodeError;

}  // namespace hotline::protocol
