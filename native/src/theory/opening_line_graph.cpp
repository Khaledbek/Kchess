#include "theory/opening_line_graph.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "chess/move.h"
#include "theory/position_key.h"

namespace kchess {
namespace {

constexpr std::size_t kHeaderSize = 96;
constexpr std::size_t kNodeSize = 20;
constexpr std::size_t kMoveSize = 2;
constexpr std::uint16_t kFormatVersion = 1;
constexpr const char* kStandardStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

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
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset,
    const std::size_t length) {
  const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(offset);
  const auto last = first + static_cast<std::ptrdiff_t>(length);
  const auto zero = std::find(first, last, 0);
  if (zero == last) throw std::runtime_error("KCL metadata is not NUL terminated");
  return std::string(first, zero);
}

}  // namespace

KclOpeningLineGraph::KclOpeningLineGraph(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Opening-line graph cannot be opened: " + path.string());
  const auto end = input.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) < kHeaderSize) {
    throw std::runtime_error("Opening-line graph has a truncated header");
  }
  const auto file_size = static_cast<std::uint64_t>(end);
  if (file_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::runtime_error("Opening-line graph is too large for this platform");
  }

  input.seekg(0);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
  if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
    throw std::runtime_error("Opening-line graph is truncated");
  }
  if (!std::equal(bytes.begin(), bytes.begin() + 4, std::array{'K', 'C', 'L', '1'}.begin())) {
    throw std::runtime_error("Opening-line graph has invalid KCL magic");
  }

  const auto version = read_u16(bytes, 4);
  if (version != kFormatVersion) throw std::runtime_error("Unsupported KCL format version");
  if (read_u16(bytes, 6) != kHeaderSize || read_u16(bytes, 8) != kNodeSize
      || read_u16(bytes, 10) != 0) {
    throw std::runtime_error("Unsupported KCL node layout");
  }
  if (read_u32(bytes, 16) != kBookMoveEncodingVersion || read_u32(bytes, 24) != kMoveSize) {
    throw std::runtime_error("Unsupported KCL move encoding");
  }
  for (std::size_t index = 88; index < kHeaderSize; ++index) {
    if (bytes[index] != 0) throw std::runtime_error("KCL header reserved bytes are nonzero");
  }

  const auto line_count = static_cast<std::uint64_t>(read_u32(bytes, 12));
  const auto move_bytes = static_cast<std::uint64_t>(read_u32(bytes, 20));
  if (move_bytes % kMoveSize != 0) {
    throw std::runtime_error("KCL move table has an invalid byte length");
  }
  if (line_count > (std::numeric_limits<std::uint64_t>::max() - kHeaderSize) / kNodeSize) {
    throw std::runtime_error("KCL node count is out of range");
  }
  const auto nodes_end = static_cast<std::uint64_t>(kHeaderSize) + line_count * kNodeSize;
  if (nodes_end > std::numeric_limits<std::uint64_t>::max() - move_bytes
      || nodes_end + move_bytes != file_size) {
    throw std::runtime_error("Opening-line graph is truncated or contains trailing data");
  }

  const auto move_count = move_bytes / kMoveSize;
  metadata_ = {
      .format_version = version,
      .node_count = line_count,
      .edge_count = move_count,
      .max_ply = read_u32(bytes, 28),
      .build_timestamp = static_cast<std::int64_t>(read_u64(bytes, 32)),
      .source = fixed_text(bytes, 40, 16),
      .license = fixed_text(bytes, 56, 16),
      .builder_version = fixed_text(bytes, 72, 16),
  };

  lines_.reserve(static_cast<std::size_t>(line_count));
  std::uint64_t previous_key = 0;
  bool has_previous = false;
  for (std::uint64_t index = 0; index < line_count; ++index) {
    const auto offset = kHeaderSize + static_cast<std::size_t>(index) * kNodeSize;
    const auto terminal_key = read_u64(bytes, offset);
    const auto persisted_byte_offset = read_u32(bytes, offset + 8);
    const auto persisted_move_count = read_u32(bytes, offset + 12);
    if (read_u32(bytes, offset + 16) != 0) {
      throw std::runtime_error("KCL node reserved bytes are nonzero");
    }
    if (has_previous && terminal_key <= previous_key) {
      throw std::runtime_error("KCL nodes are not strictly sorted");
    }
    if (persisted_byte_offset % kMoveSize != 0) {
      throw std::runtime_error("KCL node move offset is not move-aligned");
    }
    const auto first_move = static_cast<std::uint64_t>(persisted_byte_offset) / kMoveSize;
    const auto line_moves = static_cast<std::uint64_t>(persisted_move_count);
    if (first_move > move_count || line_moves > move_count - first_move) {
      throw std::runtime_error("KCL node move range is out of bounds");
    }
    if (metadata_.max_ply > 0 && line_moves > metadata_.max_ply) {
      throw std::runtime_error("KCL line exceeds declared max ply");
    }

    position_key_fingerprint_ = extend_position_key_fingerprint(
        position_key_fingerprint_, terminal_key);
    previous_key = terminal_key;
    has_previous = true;
    lines_.push_back({
        .terminal_position_key = terminal_key,
        .move_offset = static_cast<std::uint32_t>(first_move),
        .move_count = persisted_move_count,
    });
  }

  moves_.reserve(static_cast<std::size_t>(move_count));
  const auto move_table = static_cast<std::size_t>(nodes_end);
  for (std::uint64_t index = 0; index < move_count; ++index) {
    const auto encoded = read_u16(bytes, move_table + static_cast<std::size_t>(index) * kMoveSize);
    const auto decoded = decode_book_move(encoded);
    if (encode_book_move(decoded) != encoded) {
      throw std::runtime_error("KCL move does not round-trip through the shared book codec");
    }
    moves_.push_back(encoded);
  }

  // KCL1 records are root-to-terminal opening lines, not persisted outgoing
  // adjacency. Replay every line once and merge shared positions by the same
  // Stockfish key used by KCB/KCO. This makes transpositions converge naturally
  // and exposes true outgoing continuations to the training runtime.
  runtime_nodes_.reserve(static_cast<std::size_t>(line_count) * 2);
  runtime_lookup_.reserve(static_cast<std::size_t>(line_count) * 2);
  const auto start_key = stockfish_position_key(kStandardStartFen);
  ensure_runtime_node(start_key);

  for (const auto& line : lines_) {
    std::string fen = kStandardStartFen;
    std::uint64_t source_key = start_key;

    for (std::uint32_t ply = 0; ply < line.move_count; ++ply) {
      const auto encoded = moves_[line.move_offset + ply];
      const auto uci = decode_book_move(encoded);
      const auto source_node = ensure_runtime_node(source_key);

      AppliedMove played;
      try {
        played = apply_legal_uci_move(fen, uci);
      } catch (const std::invalid_argument&) {
        throw std::runtime_error(
            "KCL line contains an illegal move at ply " + std::to_string(ply + 1)
            + ": " + uci);
      }

      add_runtime_edge(source_node, encoded);
      const auto destination_key = stockfish_position_key(played.fen_after);
      ensure_runtime_node(destination_key);
      fen = played.fen_after;
      source_key = destination_key;
    }

    if (source_key != line.terminal_position_key) {
      throw std::runtime_error(
          "KCL line terminal position key does not match its replayed move sequence");
    }
  }
}

std::uint32_t KclOpeningLineGraph::ensure_runtime_node(const std::uint64_t position_key) {
  if (const auto found = runtime_lookup_.find(position_key); found != runtime_lookup_.end()) {
    return found->second;
  }
  if (runtime_nodes_.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    throw std::runtime_error("KCL runtime graph has too many positions");
  }
  const auto index = static_cast<std::uint32_t>(runtime_nodes_.size());
  runtime_nodes_.push_back({.position_key = position_key, .outgoing_moves = {}});
  runtime_lookup_.emplace(position_key, index);
  return index;
}

void KclOpeningLineGraph::add_runtime_edge(
    const std::uint32_t source_node, const std::uint16_t encoded_move) {
  auto& outgoing = runtime_nodes_.at(source_node).outgoing_moves;
  if (std::find(outgoing.begin(), outgoing.end(), encoded_move) == outgoing.end()) {
    outgoing.push_back(encoded_move);
  }
}

std::optional<std::uint32_t> KclOpeningLineGraph::node_index(
    const std::uint64_t position_key) const {
  const auto found = runtime_lookup_.find(position_key);
  if (found == runtime_lookup_.end()) return std::nullopt;
  return found->second;
}

std::vector<OpeningLineContinuation> KclOpeningLineGraph::continuations(
    const std::string& fen) const {
  const auto source_index = node_index(stockfish_position_key(fen));
  if (!source_index.has_value()) return {};

  const auto& node = runtime_nodes_.at(*source_index);
  std::vector<OpeningLineContinuation> result;
  result.reserve(node.outgoing_moves.size());
  for (const auto encoded : node.outgoing_moves) {
    const auto uci = decode_book_move(encoded);
    try {
      const auto played = apply_legal_uci_move(fen, uci);
      const auto destination_key = stockfish_position_key(played.fen_after);
      result.push_back({
          .uci = played.uci,
          .san = played.san,
          .fen_after = played.fen_after,
          .destination_key = destination_key,
          .destination_node = node_index(destination_key),
      });
    } catch (const std::invalid_argument&) {
      // Position-key collisions are astronomically unlikely, but a concrete
      // caller FEN remains authoritative. Never expose an illegal continuation.
    }
  }
  return result;
}

std::string KclOpeningLineGraph::source_version() const {
  return "kcl" + std::to_string(metadata_.format_version) + ":" + metadata_.source + ":"
      + std::to_string(metadata_.build_timestamp) + ":" + metadata_.builder_version;
}

}  // namespace kchess
