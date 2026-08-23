// AppWarrior Core: 'IVA1' flattened ID-var-array decoder.
//
// Bounded, validated reader for the historical UIDVarArray flattened
// format (legacy AppWarrior/Source/Data/UIDVarArray.cpp:5-26, 391-473) —
// the storage format of the AppWarrior error catalogs ("Error Msgs/*.dat",
// loaded via 'EMSG' resources on Mac and AWRZ files on Windows).
//
// Layout (all multi-byte values big-endian):
//
//   u32 format;               // 'IVA1'
//   u32 rsvd;                 // zero
//   u32 textEncoding;         // used if the items are text (0 in shipped files)
//   u32 itemCount;
//   struct { u32 id; u32 offset; } offsetTab[itemCount + 1];
//                             // extra entry: id ignored (zero), offset = total data size
//   u8 data[];                // item i occupies [offsetTab[i].offset, offsetTab[i+1].offset)
//
// Safety validation mirrors the legacy Unflatten: format tag, the
// itemCount & 0xFF000000 overflow guard, and monotonic in-bounds offsets
// are all enforced as DecodeErrors.
//
// Deliberate, documented leniency: the legacy Unflatten ALSO required
// strictly increasing IDs, but the shipped error catalog
// "AppWarrior/Error Msgs/UError(1).dat" violates that rule — its offset
// table is ..., 9, 13, 11, 12, 13 (duplicate 13, out of order). The legacy
// Windows loader used the non-validating static UIDVarArray::GetItem, so
// the file shipped broken and legacy Unflatten itself would reject it. To
// keep every shipped asset readable, this decoder preserves table order,
// permits duplicate/unsorted IDs, and `find`/`item_data` return the FIRST
// match — matching the legacy lookup's intent, not its mis-binary-search.

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "appwarrior/core/endian.h"

namespace appwarrior::ivar {

inline constexpr std::uint32_t kFormatTag = appwarrior::endian::four_cc('I', 'V', 'A', '1');
inline constexpr std::size_t kMinSize = 16 + 8;  // header + one table entry

static_assert(kMinSize == 24);

enum class DecodeError {
  truncated,             // fewer than 24 bytes, or the declared table overruns the buffer
  wrong_format_tag,      // first u32 != 'IVA1'
  impossible_item_count,  // itemCount & 0xFF000000 (legacy overflow guard)
  offset_out_of_range,   // a table offset is below the previous one or beyond the data
};

struct Item {
  std::uint32_t id = 0;
  std::vector<std::byte> data;
};

struct Array {
  std::uint32_t text_encoding = 0;
  std::vector<Item> items;  // table order (see the leniency note above)
};

// Never throws, never reads out of bounds.
[[nodiscard]] auto decode(std::span<const std::byte> bytes) -> std::expected<Array, DecodeError>;

// First item with the given ID, or nullptr (IDs are normally unique; the
// shipped UError(1).dat catalog is the documented exception).
[[nodiscard]] auto find(const Array& array, std::uint32_t id) noexcept -> const Item*;

// Data of the first item with the given ID; empty span if absent.
[[nodiscard]] auto item_data(const Array& array, std::uint32_t id) noexcept
    -> std::span<const std::byte>;

}  // namespace appwarrior::ivar
