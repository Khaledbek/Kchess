#include "theory/opening_name_index.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

#include "theory/position_key.h"

namespace kchess {
namespace {

constexpr std::size_t kHeaderSize = 96;
constexpr std::size_t kEntrySize = 20;
constexpr std::uint16_t kFormatVersion = 1;

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset])
      | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
  std::uint32_t value = 0;
  for (int index = 0; index < 4; ++index) {
    value |= static_cast<std::uint32_t>(bytes[offset + index]) << (8 * index);
  }
  return value;
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
  std::uint64_t value = 0;
  for (int index = 0; index < 8; ++index) {
    value |= static_cast<std::uint64_t>(bytes[offset + index]) << (8 * index);
  }
  return value;
}

std::string fixed_text(
    const std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::size_t length) {
  const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(offset);
  const auto last = first + static_cast<std::ptrdiff_t>(length);
  const auto zero = std::find(first, last, 0);
  if (zero == last) throw std::runtime_error("KCO metadata is not NUL terminated");
  return std::string(first, zero);
}

std::string decode_eco(
    const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
  std::string eco;
  for (std::size_t index = 0; index < 4; ++index) {
    const auto character = bytes[offset + index];
    if (character == 0) break;
    eco.push_back(static_cast<char>(character));
  }
  return eco;
}

}  // namespace

KcoOpeningNameIndex::KcoOpeningNameIndex(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Opening-name index cannot be opened: " + path.string());
  const auto end = input.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) < kHeaderSize) {
    throw std::runtime_error("Opening-name index has a truncated header");
  }
  const auto file_size = static_cast<std::uint64_t>(end);
  if (file_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::runtime_error("Opening-name index is too large for this platform");
  }
  input.seekg(0);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
  if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
    throw std::runtime_error("Opening-name index is truncated");
  }
  if (!std::equal(bytes.begin(), bytes.begin() + 4, std::array{'K', 'C', 'O', '1'}.begin())) {
    throw std::runtime_error("Opening-name index has invalid KCO magic");
  }
  const auto version = read_u16(bytes, 4);
  if (version != kFormatVersion) throw std::runtime_error("Unsupported KCO format version");
  if (read_u16(bytes, 6) != kHeaderSize || read_u16(bytes, 8) != kEntrySize
      || read_u16(bytes, 10) != 0) {
    throw std::runtime_error("Unsupported KCO layout");
  }
  for (std::size_t index = 88; index < kHeaderSize; ++index) {
    if (bytes[index] != 0) throw std::runtime_error("KCO header reserved bytes are nonzero");
  }
  const auto entry_count = read_u64(bytes, 12);
  const auto table_size = read_u64(bytes, 20);
  if (entry_count > (std::numeric_limits<std::uint64_t>::max() - kHeaderSize) / kEntrySize) {
    throw std::runtime_error("Opening-name index entry count is out of range");
  }
  const auto entries_end = kHeaderSize + entry_count * kEntrySize;
  if (entries_end > std::numeric_limits<std::uint64_t>::max() - table_size
      || entries_end + table_size != file_size) {
    throw std::runtime_error("Opening-name index is truncated or contains trailing data");
  }
  metadata_ = {
      .format_version = version,
      .entry_count = entry_count,
      .max_ply = read_u32(bytes, 28),
      .build_timestamp = static_cast<std::int64_t>(read_u64(bytes, 32)),
      .source = fixed_text(bytes, 40, 16),
      .license = fixed_text(bytes, 56, 16),
      .builder_version = fixed_text(bytes, 72, 16),
  };
  string_table_.assign(
      reinterpret_cast<const char*>(bytes.data()) + entries_end,
      static_cast<std::size_t>(table_size));

  entries_.reserve(static_cast<std::size_t>(entry_count));
  std::uint64_t previous_key = 0;
  bool has_previous = false;
  for (std::uint64_t index = 0; index < entry_count; ++index) {
    const auto offset = kHeaderSize + static_cast<std::size_t>(index) * kEntrySize;
    if (read_u16(bytes, offset + 18) != 0) {
      throw std::runtime_error("KCO entry reserved bytes are nonzero");
    }
    Entry entry{
        .position_key = read_u64(bytes, offset),
        .name_offset = read_u32(bytes, offset + 8),
        .ply = read_u16(bytes, offset + 16),
        .eco = decode_eco(bytes, offset + 12),
    };
    if (has_previous && entry.position_key <= previous_key) {
      throw std::runtime_error("KCO entries are not strictly sorted");
    }
    if (entry.name_offset >= string_table_.size()
        || string_table_.find('\0', entry.name_offset) == std::string::npos) {
      throw std::runtime_error("KCO entry name offset is invalid");
    }
    previous_key = entry.position_key;
    has_previous = true;
    entries_.push_back(std::move(entry));
  }
}

std::optional<OpeningName> KcoOpeningNameIndex::lookup(const std::uint64_t position_key) const {
  const auto found = std::lower_bound(
      entries_.begin(), entries_.end(), position_key,
      [](const Entry& entry, const std::uint64_t value) { return entry.position_key < value; });
  if (found == entries_.end() || found->position_key != position_key) return std::nullopt;
  const auto terminator = string_table_.find('\0', found->name_offset);
  return OpeningName{
      .eco = found->eco,
      .name = string_table_.substr(
          found->name_offset, terminator - found->name_offset),
      .ply = found->ply,
  };
}

std::string KcoOpeningNameIndex::source_version() const {
  return "kco" + std::to_string(metadata_.format_version) + ":" + metadata_.source + ":"
      + std::to_string(metadata_.build_timestamp) + ":" + metadata_.builder_version;
}

std::optional<OpeningName> classify_opening(
    const OpeningNameIndex& index, const std::vector<ParsedMove>& moves) {
  const std::uint32_t cap = index.max_ply();
  if (cap == 0) return std::nullopt;
  const std::size_t limit = std::min<std::size_t>(moves.size(), cap);
  std::optional<OpeningName> best;
  for (std::size_t ply = 0; ply < limit; ++ply) {
    auto found = index.lookup(stockfish_position_key(moves[ply].fen_after));
    if (found.has_value()) best = std::move(found);
  }
  return best;
}

}  // namespace kchess
