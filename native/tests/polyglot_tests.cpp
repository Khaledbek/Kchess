// -----------------------------------------------------------------------------
// Section: PolyGlot Zobrist key contract, move decoding and weighted sampling
// -----------------------------------------------------------------------------

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "theory/polyglot.h"

namespace {

void expect(const bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

struct Vector {
  const char* fen;
  std::uint64_t key;
};

// The reference keys published with the PolyGlot book format. A book lookup
// silently returns nothing when these drift, so they are worth pinning.
void test_reference_keys() {
  const std::vector<Vector> vectors = {
      {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
          0x463b96181691fc9cULL},
      // En passant is in the FEN but no black pawn can take on e3.
      {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
          0x823c9b50fd114196ULL},
      {"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",
          0x0756b94461c50fb0ULL},
      {"rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2",
          0x662fafb965db29d4ULL},
      // Here the e-pawn really can take on f6, so the file counts.
      {"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
          0x22a48b5a8e47ff78ULL},
      {"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR b kq - 0 3",
          0x652a607ca3f242c1ULL},
      {"rnbq1bnr/ppp1pkpp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR w - - 0 4",
          0x00fdd303c946bdd9ULL},
      {"rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq c3 0 3",
          0x3c8123ea7b067637ULL},
  };
  for (const auto& item : vectors) {
    const auto key = kchess::polyglot_position_key(item.fen);
    expect(key == item.key,
        std::string("Wrong PolyGlot key for ") + item.fen);
  }
}

// Colour is part of the piece index, so mirrored positions must differ.
void test_colours_are_distinct() {
  const auto white = kchess::polyglot_position_key(
      "4k3/8/8/8/8/8/8/4K2R w K - 0 1");
  const auto black = kchess::polyglot_position_key(
      "4k2r/8/8/8/8/8/8/4K3 b k - 0 1");
  expect(white != black, "Mirrored positions must not share a key");
}

// A throwaway one-position book, so decoding can be exercised without the
// 170 MB drill book that is not in version control.
class TemporaryBook {
 public:
  TemporaryBook(const std::string& fen,
                const std::vector<std::pair<std::uint16_t, std::uint16_t>>& moves)
      : path_(std::filesystem::temp_directory_path() /
              ("kchess_book_" + std::to_string(std::hash<std::string>{}(fen)) + ".bin")) {
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    const auto key = kchess::polyglot_position_key(fen);
    for (const auto& [move, weight] : moves) {
      write_big_endian(out, key, 8);
      write_big_endian(out, move, 2);
      write_big_endian(out, weight, 2);
      write_big_endian(out, 0, 4);
    }
  }
  ~TemporaryBook() {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }
  const std::filesystem::path& path() const { return path_; }

 private:
  static void write_big_endian(std::ofstream& out, std::uint64_t value, int bytes) {
    for (int index = bytes - 1; index >= 0; --index) {
      const auto byte = static_cast<unsigned char>((value >> (index * 8)) & 0xFF);
      out.put(static_cast<char>(byte));
    }
  }
  std::filesystem::path path_;
};

constexpr std::uint16_t encode(int from_file, int from_rank, int to_file, int to_rank) {
  return static_cast<std::uint16_t>(
      to_file | (to_rank << 3) | (from_file << 6) | (from_rank << 9));
}

// PolyGlot writes castling as the king capturing its own rook. Left alone it
// decodes to e1h1, which no move generator accepts, so the drill would silently
// drop every castling move from the book.
void test_castling_decodes_to_the_kings_move() {
  const std::string fen = "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1";
  const TemporaryBook book(fen, {{encode(4, 0, 7, 0), 100}, {encode(4, 0, 0, 0), 50}});
  kchess::PolyglotBook opened(book.path());
  const auto moves = opened.ranked_moves(fen);
  expect(moves.size() == 2, "Both book entries for the position are returned");
  expect(moves[0].uci == "e1g1", "King side castling decodes to e1g1, not e1h1");
  expect(moves[1].uci == "e1c1", "Queen side castling decodes to e1c1, not e1a1");
}

// The same encoding is an ordinary capture when the squares do not hold a king
// and its own rook, and must survive untouched.
void test_rook_captures_are_not_rewritten() {
  // A white rook on e1, a black rook on h1: e1h1 is a capture, not castling.
  const std::string capture_fen = "4k3/8/8/8/8/8/8/4R2r w - - 0 1";
  const TemporaryBook captures(capture_fen, {{encode(4, 0, 7, 0), 100}});
  kchess::PolyglotBook opened_captures(captures.path());
  expect(opened_captures.ranked_moves(capture_fen).at(0).uci == "e1h1",
      "A rook capture on h1 keeps its own target square");

  // A white king on e1 taking a black rook on h1 is still a capture.
  const std::string king_fen = "4k3/8/8/8/8/8/8/4K2r w - - 0 1";
  const TemporaryBook king(king_fen, {{encode(4, 0, 7, 0), 100}});
  kchess::PolyglotBook opened_king(king.path());
  expect(opened_king.ranked_moves(king_fen).at(0).uci == "e1h1",
      "Taking an enemy rook is not castling");
}

// The opponent in a drill is sampled, not scripted: a heavy move dominates but
// never shuts the rest of the book out.
void test_weighted_sampling_follows_the_weights() {
  std::mt19937 random(1234);
  const std::vector<kchess::BookMove> moves = {{"e2e4", 900}, {"d2d4", 100}};
  int heavy = 0;
  int light = 0;
  for (int draw = 0; draw < 2000; ++draw) {
    const auto* picked = kchess::pick_weighted(moves, random);
    expect(picked != nullptr, "A non-empty set always yields a move");
    if (picked->uci == "e2e4") ++heavy; else ++light;
  }
  expect(heavy > light * 4, "The heavier move is drawn far more often");
  expect(light > 0, "The lighter move is still drawn sometimes");

  const std::vector<kchess::BookMove> unweighted = {{"e2e4", 0}, {"d2d4", 0}};
  bool saw_second = false;
  for (int draw = 0; draw < 200; ++draw) {
    const auto* picked = kchess::pick_weighted(unweighted, random);
    expect(picked != nullptr, "An all-zero set still yields a move");
    if (picked->uci == "d2d4") saw_second = true;
  }
  expect(saw_second, "An all-zero set is drawn uniformly, not always the first");

  const std::vector<kchess::BookMove> empty;
  expect(kchess::pick_weighted(empty, random) == nullptr,
      "An empty set has nothing to draw");
}

}  // namespace

int main() {
  try {
    test_reference_keys();
    test_colours_are_distinct();
    test_castling_decodes_to_the_kings_move();
    test_rook_captures_are_not_rewritten();
    test_weighted_sampling_follows_the_weights();
  } catch (const std::exception& error) {
    std::cerr << "polyglot tests failed: " << error.what() << std::endl;
    return 1;
  }
  std::cout << "polyglot tests passed" << std::endl;
  return 0;
}
