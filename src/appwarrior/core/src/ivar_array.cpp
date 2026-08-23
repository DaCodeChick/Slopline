#include "appwarrior/core/ivar_array.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "appwarrior/core/endian.h"

namespace aw::ivar {

auto decode(std::span<const std::byte> bytes) -> std::expected<Array, DecodeError> {
  if (bytes.size() < kMinSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (aw::endian::read_u32be(bytes.first<4>()) != kFormatTag) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }

  const std::uint32_t item_count =
      aw::endian::read_u32be(bytes.subspan(12).first<4>());
  if ((item_count & 0xFF000000U) != 0) {
    return std::unexpected(DecodeError::impossible_item_count);
  }

  // The table holds item_count + 1 entries of {u32 id, u32 offset}.
  const std::size_t table_size = (static_cast<std::size_t>(item_count) + 1U) * 8U;
  const std::size_t header_and_table = 16U + table_size;
  if (bytes.size() < header_and_table) {
    return std::unexpected(DecodeError::truncated);
  }

  const std::span<const std::byte> table = bytes.subspan(16, table_size);
  const std::span<const std::byte> data = bytes.subspan(header_and_table);

  // First pass: validate the offsets (bounds safety — strictly monotonic
  // within the data area) and collect them. IDs are deliberately not
  // order-validated; see the header's leniency note.
  std::vector<std::uint32_t> offsets;
  offsets.reserve(static_cast<std::size_t>(item_count) + 1U);

  std::uint32_t previous_offset = 0;
  for (std::uint32_t i = 0; i <= item_count; ++i) {
    const std::span<const std::byte> entry =
        table.subspan(static_cast<std::size_t>(i) * 8U).first<8>();
    const std::uint32_t offset = aw::endian::read_u32be(entry.subspan(4).first<4>());
    if (offset < previous_offset || offset > data.size()) {
      return std::unexpected(DecodeError::offset_out_of_range);
    }
    previous_offset = offset;
    offsets.push_back(offset);
  }

  // Second pass: slice the items.
  Array array;
  array.text_encoding = aw::endian::read_u32be(bytes.subspan(8).first<4>());
  array.items.reserve(item_count);

  for (std::uint32_t i = 0; i < item_count; ++i) {
    const std::uint32_t id =
        aw::endian::read_u32be(table.subspan(static_cast<std::size_t>(i) * 8U).first<4>());

    const std::size_t begin = static_cast<std::size_t>(offsets[i]);
    const std::size_t end = static_cast<std::size_t>(offsets[static_cast<std::size_t>(i) + 1U]);

    Item item;
    item.id = id;
    const std::span<const std::byte> item_bytes = data.subspan(begin, end - begin);
    item.data.assign(item_bytes.begin(), item_bytes.end());
    array.items.push_back(std::move(item));
  }

  return array;
}

auto find(const Array& array, std::uint32_t id) noexcept -> const Item* {
  for (const Item& item : array.items) {
    if (item.id == id) {
      return &item;
    }
  }
  return nullptr;
}

auto item_data(const Array& array, std::uint32_t id) noexcept -> std::span<const std::byte> {
  const Item* item = find(array, id);
  return item != nullptr ? std::span<const std::byte>(item->data) : std::span<const std::byte>{};
}

}  // namespace aw::ivar
