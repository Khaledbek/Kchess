#include "theory/polyglot.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace kchess {
namespace {

// PolyGlot's kind_of_piece: black first, then white, per piece type.
int piece_index(char c) {
  switch (c) {
    case 'p': return 0;
    case 'P': return 1;
    case 'n': return 2;
    case 'N': return 3;
    case 'b': return 4;
    case 'B': return 5;
    case 'r': return 6;
    case 'R': return 7;
    case 'q': return 8;
    case 'Q': return 9;
    case 'k': return 10;
    case 'K': return 11;
    default: return -1;
  }
}

// squares[rank * 8 + file], rank 0 being the first rank. The hash and the move
// decoder both read the board, so they both read it through here.
std::array<char, 64> board_squares(const std::string& placement) {
  std::array<char, 64> squares{};
  int rank = 7;
  int file = 0;
  for (char c : placement) {
    if (c == '/') {
      rank--;
      file = 0;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      file += c - '0';
    } else {
      if (rank >= 0 && rank < 8 && file >= 0 && file < 8) squares[rank * 8 + file] = c;
      file++;
    }
  }
  return squares;
}

}  // namespace

std::uint64_t polyglot_position_key(const std::string& fen) {
  std::istringstream iss(fen);
  std::string board, color, castling, en_passant;
  iss >> board >> color >> castling >> en_passant;

  const auto squares = board_squares(board);

  std::uint64_t hash = 0;
  for (int square = 0; square < 64; ++square) {
    const int piece = piece_index(squares[square]);
    if (piece >= 0) hash ^= PolyglotRandomArray[piece * 64 + square];
  }

  if (castling.find('K') != std::string::npos) hash ^= PolyglotRandomArray[768];
  if (castling.find('Q') != std::string::npos) hash ^= PolyglotRandomArray[769];
  if (castling.find('k') != std::string::npos) hash ^= PolyglotRandomArray[770];
  if (castling.find('q') != std::string::npos) hash ^= PolyglotRandomArray[771];

  // PolyGlot only hashes the en passant file when a pawn of the side to move
  // can really capture there. A FEN lists the square after every double push,
  // so hashing it unconditionally misses the book for most positions.
  if (en_passant.size() >= 2 && en_passant != "-") {
    const int ep_file = en_passant[0] - 'a';
    const int ep_rank = en_passant[1] - '1';
    if (ep_file >= 0 && ep_file < 8 && ep_rank >= 0 && ep_rank < 8) {
      const bool white_to_move = color == "w";
      const int capture_rank = white_to_move ? ep_rank - 1 : ep_rank + 1;
      const char pawn = white_to_move ? 'P' : 'p';
      bool capturable = false;
      if (capture_rank >= 0 && capture_rank < 8) {
        for (const int neighbour : {ep_file - 1, ep_file + 1}) {
          if (neighbour < 0 || neighbour > 7) continue;
          if (squares[capture_rank * 8 + neighbour] == pawn) capturable = true;
        }
      }
      if (capturable) hash ^= PolyglotRandomArray[772 + ep_file];
    }
  }

  if (color == "w") hash ^= PolyglotRandomArray[780];

  return hash;
}

template <typename T>
T swap_endian(T u) {
  union {
    T u;
    unsigned char u8[sizeof(T)];
  } source, dest;
  source.u = u;
  for (size_t k = 0; k < sizeof(T); k++)
    dest.u8[k] = source.u8[sizeof(T) - k - 1];
  return dest.u;
}

PolyglotBook::PolyglotBook(const std::filesystem::path& path)
    : file_(path, std::ios::binary | std::ios::ate), num_entries_(0) {
  if (!file_) {
    throw std::runtime_error("Could not open Polyglot book: " + path.string());
  }
  auto size = file_.tellg();
  if (size % 16 != 0) {
    throw std::runtime_error("Polyglot book size is not a multiple of 16 bytes.");
  }
  num_entries_ = size / 16;
}

std::optional<PolyglotEntry> PolyglotBook::find_first_entry(std::uint64_t key) {
  if (num_entries_ == 0) return std::nullopt;

  std::uint64_t low = 0;
  std::uint64_t high = num_entries_ - 1;
  std::uint64_t first_match = num_entries_;

  while (low <= high) {
    std::uint64_t mid = low + (high - low) / 2;
    file_.seekg(mid * 16, std::ios::beg);
    
    std::uint64_t mid_key;
    if (!file_.read(reinterpret_cast<char*>(&mid_key), 8)) break;
    mid_key = swap_endian(mid_key);

    if (mid_key < key) {
      low = mid + 1;
    } else if (mid_key > key) {
      if (mid == 0) break;
      high = mid - 1;
    } else {
      first_match = mid;
      if (mid == 0) break;
      high = mid - 1; // Keep searching left for the first occurrence
    }
  }

  if (first_match < num_entries_) {
    file_.seekg(first_match * 16, std::ios::beg);
    PolyglotEntry entry;
    if (file_.read(reinterpret_cast<char*>(&entry.key), 8) &&
        file_.read(reinterpret_cast<char*>(&entry.move), 2) &&
        file_.read(reinterpret_cast<char*>(&entry.weight), 2) &&
        file_.read(reinterpret_cast<char*>(&entry.learn), 4)) {
      entry.key = swap_endian(entry.key);
      entry.move = swap_endian(entry.move);
      entry.weight = swap_endian(entry.weight);
      entry.learn = swap_endian(entry.learn);
      return entry;
    }
  }
  return std::nullopt;
}

std::vector<PolyglotEntry> PolyglotBook::entries_for(std::uint64_t key) {
  // Entries for one key are contiguous, so the first match anchors the scan.
  auto first_entry = find_first_entry(key);
  if (!first_entry) return {};

  const std::uint64_t start_index = static_cast<std::uint64_t>(file_.tellg()) / 16 - 1;
  file_.seekg(start_index * 16, std::ios::beg);

  std::vector<PolyglotEntry> entries;
  while (true) {
    PolyglotEntry entry;
    if (!file_.read(reinterpret_cast<char*>(&entry.key), 8) ||
        !file_.read(reinterpret_cast<char*>(&entry.move), 2) ||
        !file_.read(reinterpret_cast<char*>(&entry.weight), 2) ||
        !file_.read(reinterpret_cast<char*>(&entry.learn), 4)) {
      break;
    }
    entry.key = swap_endian(entry.key);
    entry.move = swap_endian(entry.move);
    entry.weight = swap_endian(entry.weight);
    entry.learn = swap_endian(entry.learn);
    if (entry.key != key) break;
    entries.push_back(entry);
  }

  std::sort(entries.begin(), entries.end(),
      [](const PolyglotEntry& a, const PolyglotEntry& b) { return a.weight > b.weight; });
  return entries;
}

std::vector<BookMove> PolyglotBook::ranked_moves(const std::string& fen, int max_moves) {
  const auto entries = entries_for(polyglot_position_key(fen));
  std::vector<BookMove> moves;
  for (const auto& entry : entries) {
    if (max_moves > 0 && static_cast<int>(moves.size()) >= max_moves) break;
    moves.push_back({decode_move(entry.move, fen), entry.weight});
  }
  return moves;
}

std::string PolyglotBook::decode_move(std::uint16_t poly_move, const std::string& fen) const {
  const int to_file = poly_move & 7;
  const int to_rank = (poly_move >> 3) & 7;
  const int from_file = (poly_move >> 6) & 7;
  const int from_rank = (poly_move >> 9) & 7;
  const int promo = (poly_move >> 12) & 7;

  // PolyGlot stores castling as the king capturing its own rook (e1h1), which
  // no move generator accepts. Rewrite it to the king's two-square move, but
  // only when those squares really hold that king and rook: an ordinary rook
  // capture on h1 encodes identically and has to survive untouched.
  int landing_file = to_file;
  std::istringstream input(fen);
  std::string placement;
  input >> placement;
  const auto squares = board_squares(placement);
  const char mover = squares[from_rank * 8 + from_file];
  const char captured = squares[to_rank * 8 + to_file];
  if (from_rank == to_rank &&
      ((mover == 'K' && captured == 'R') || (mover == 'k' && captured == 'r'))) {
    landing_file = to_file > from_file ? 6 : 2;
  }

  std::string uci = "";
  uci += static_cast<char>('a' + from_file);
  uci += static_cast<char>('1' + from_rank);
  uci += static_cast<char>('a' + landing_file);
  uci += static_cast<char>('1' + to_rank);

  if (promo > 0) {
    const char promos[] = {'?', 'n', 'b', 'r', 'q'};
    if (promo <= 4) uci += promos[promo];
  }

  return uci;
}

}  // namespace kchess
