#include "theory/opening_theory_provider.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "theory/position_key.h"

namespace kchess {
namespace {

constexpr std::size_t kHeaderSize = 160;
constexpr std::size_t kEntrySize = 28;
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
  if (zero == last) throw std::runtime_error("KCB metadata is not NUL terminated");
  return std::string(first, zero);
}

}  // namespace

KcbOpeningTheoryProvider::KcbOpeningTheoryProvider(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Opening book cannot be opened: " + path.string());
  const auto end = input.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) < kHeaderSize) {
    throw std::runtime_error("Opening book has a truncated header");
  }
  const auto file_size = static_cast<std::uint64_t>(end);
  if (file_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::runtime_error("Opening book is too large for this platform");
  }
  input.seekg(0);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
  if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
    throw std::runtime_error("Opening book is truncated");
  }
  if (!std::equal(bytes.begin(), bytes.begin() + 4, std::array{'K', 'C', 'B', '1'}.begin())) {
    throw std::runtime_error("Opening book has invalid KCB magic");
  }
  const auto version = read_u16(bytes, 4);
  if (version != kFormatVersion) throw std::runtime_error("Unsupported KCB format version");
  if (read_u16(bytes, 6) != kHeaderSize || read_u16(bytes, 8) != kEntrySize
      || read_u16(bytes, 10) != 0) {
    throw std::runtime_error("Unsupported KCB layout");
  }
  for (std::size_t index = 148; index < kHeaderSize; ++index) {
    if (bytes[index] != 0) throw std::runtime_error("KCB header reserved bytes are nonzero");
  }
  const auto entry_count = read_u64(bytes, 12);
  if (entry_count > (std::numeric_limits<std::uint64_t>::max() - kHeaderSize) / kEntrySize
      || kHeaderSize + entry_count * kEntrySize != file_size) {
    throw std::runtime_error("Opening book is truncated or contains trailing data");
  }
  metadata_ = {
      .format_version = version,
      .entry_count = entry_count,
      .max_ply = read_u32(bytes, 20),
      .min_games = read_u32(bytes, 24),
      .build_timestamp = static_cast<std::int64_t>(read_u64(bytes, 28)),
      .source = fixed_text(bytes, 36, 16),
      .license = fixed_text(bytes, 52, 16),
      .source_dump = fixed_text(bytes, 68, 64),
      .builder_version = fixed_text(bytes, 132, 16),
  };
  entries_.reserve(static_cast<std::size_t>(entry_count));
  std::pair<std::uint64_t, std::uint16_t> previous{};
  bool has_previous = false;
  for (std::uint64_t index = 0; index < entry_count; ++index) {
    const auto offset = kHeaderSize + static_cast<std::size_t>(index) * kEntrySize;
    Entry entry{
        .position_key = read_u64(bytes, offset),
        .move = read_u16(bytes, offset + 8),
        .games = read_u32(bytes, offset + 12),
        .white_wins = read_u32(bytes, offset + 16),
        .draws = read_u32(bytes, offset + 20),
        .black_wins = read_u32(bytes, offset + 24),
    };
    if (read_u16(bytes, offset + 10) != 0) {
      throw std::runtime_error("KCB entry reserved bytes are nonzero");
    }
    (void)decode_book_move(entry.move);
    const auto identity = std::pair{entry.position_key, entry.move};
    if (has_previous && identity <= previous) {
      throw std::runtime_error("KCB entries are not strictly sorted");
    }
    const std::uint64_t result_total = static_cast<std::uint64_t>(entry.white_wins)
        + entry.draws + entry.black_wins;
    if (result_total != entry.games || entry.games < metadata_.min_games) {
      throw std::runtime_error("KCB entry has inconsistent game counters");
    }
    previous = identity;
    has_previous = true;
    entries_.push_back(entry);
  }
}

TheoryMoveInfo KcbOpeningTheoryProvider::lookup(
    const std::string& fen, const std::string& uci_move) const {
  const auto identity = std::pair{stockfish_position_key(fen), encode_book_move(uci_move)};
  const auto found = std::lower_bound(
      entries_.begin(), entries_.end(), identity,
      [](const Entry& entry, const auto& value) {
        return std::pair{entry.position_key, entry.move} < value;
      });
  if (found == entries_.end() || std::pair{found->position_key, found->move} != identity) {
    return {};
  }
  return {
      .is_theory = true,
      .games = found->games,
      .white_wins = found->white_wins,
      .draws = found->draws,
      .black_wins = found->black_wins,
  };
}

std::string KcbOpeningTheoryProvider::source_version() const {
  return "kcb" + std::to_string(metadata_.format_version) + ":" + metadata_.source_dump + ":"
      + std::to_string(metadata_.build_timestamp) + ":" + metadata_.builder_version;
}

}  // namespace kchess
