#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "chess/pgn.h"

namespace kchess {

// The named opening (ECO + name) that a line of moves ends in.
struct OpeningName {
  std::string eco;
  std::string name;
  int ply{0};
};

struct OpeningNameMetadata {
  std::uint16_t format_version{0};
  std::uint64_t entry_count{0};
  std::uint32_t max_ply{0};
  std::int64_t build_timestamp{0};
  std::string source;
  std::string license;
  std::string builder_version;
};

// Maps a canonical Stockfish position key to the named opening ending at it.
// Backed by the KCO1 asset built from the CC0 lichess-org/chess-openings
// catalog (see tools/opening_names/name_format.md).
class OpeningNameIndex {
 public:
  virtual ~OpeningNameIndex() = default;
  virtual std::optional<OpeningName> lookup(std::uint64_t position_key) const = 0;
  virtual std::uint32_t max_ply() const = 0;
  virtual std::string source_version() const = 0;
  virtual OpeningNameMetadata metadata() const = 0;
};

class UnavailableOpeningNameIndex final : public OpeningNameIndex {
 public:
  std::optional<OpeningName> lookup(std::uint64_t) const override { return std::nullopt; }
  std::uint32_t max_ply() const override { return 0; }
  std::string source_version() const override { return "not-installed"; }
  OpeningNameMetadata metadata() const override { return {}; }
};

class KcoOpeningNameIndex final : public OpeningNameIndex {
 public:
  explicit KcoOpeningNameIndex(const std::filesystem::path& path);

  std::optional<OpeningName> lookup(std::uint64_t position_key) const override;
  std::uint32_t max_ply() const override { return metadata_.max_ply; }
  std::string source_version() const override;
  OpeningNameMetadata metadata() const override { return metadata_; }

 private:
  struct Entry {
    std::uint64_t position_key{0};
    std::uint32_t name_offset{0};
    std::uint16_t ply{0};
    std::string eco;
  };

  OpeningNameMetadata metadata_;
  std::vector<Entry> entries_;
  std::string string_table_;
};

// Classify a game by walking its plies, keying each resulting position and
// keeping the deepest ply that matches an index entry. That is the most
// specific named opening the game entered; move-order transpositions match
// because identity is the position, not the move sequence. Returns nullopt when
// no ply matches (for example an empty game or an unindexed line from move one).
std::optional<OpeningName> classify_opening(
    const OpeningNameIndex& index, const std::vector<ParsedMove>& moves);

}  // namespace kchess
