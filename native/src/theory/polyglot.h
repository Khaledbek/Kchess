#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include "core/weighted_choice.h"

namespace kchess {

extern const std::uint64_t PolyglotRandomArray[781];

// Computes the standard PolyGlot Zobrist hash for a given FEN.
std::uint64_t polyglot_position_key(const std::string& fen);

// One book reply with the weight the book gives it.
struct BookMove {
  std::string uci;
  std::uint16_t weight;
};

struct PolyglotEntry {
  std::uint64_t key;
  std::uint16_t move;
  std::uint16_t weight;
  std::uint32_t learn;
};

// Represents a PolyGlot .bin engine book.
// Does not load the whole book into memory; uses fast binary search via seekg.
class PolyglotBook {
 public:
  explicit PolyglotBook(const std::filesystem::path& path);

  // Every book reply for the position, heaviest first; empty when the position
  // is not in the book. max_moves <= 0 returns all of them. Moves come back as
  // UCI, castling as the king's two-square move rather than PolyGlot's
  // king-takes-rook encoding.
  std::vector<BookMove> ranked_moves(const std::string& fen, int max_moves = 0);

 private:
  std::vector<PolyglotEntry> entries_for(std::uint64_t key);
  std::optional<PolyglotEntry> find_first_entry(std::uint64_t key);
  std::string decode_move(std::uint16_t poly_move, const std::string& fen) const;

  std::ifstream file_;
  std::uint64_t num_entries_;
};

}  // namespace kchess
