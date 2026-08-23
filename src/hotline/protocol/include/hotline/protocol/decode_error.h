// Hotline wire protocol: decode/encode error codes.
//
// The protocol re-exports the framework's generic errors: decoding and
// encoding are std::expected<T, Error>, never throwing, never out of
// bounds (see appwarrior/core/decode_error.h and encode_error.h).

#pragma once

#include "appwarrior/core/decode_error.h"
#include "appwarrior/core/encode_error.h"

namespace hotline::protocol {

using DecodeError = aw::DecodeError;
using EncodeError = aw::EncodeError;

}  // namespace hotline::protocol
