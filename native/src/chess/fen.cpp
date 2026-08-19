#include "chess/fen.h"

#include <array>
#include <cctype>
#include <charconv>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

namespace kchess {
namespace {

bool parse_nonnegative(const std::string& value, int& parsed) {
  if (value.empty()) {
    return false;
  }
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  return error == std::errc{} && end == value.data() + value.size() && parsed >= 0;
}

bool is_piece(const char value) {
  constexpr std::string_view pieces = "PNBRQKpnbrqk";
  return pieces.find(value) != std::string_view::npos;
}

int square_index(const char file, const char rank) {
  return (rank - '1') * 8 + (file - 'a');
}

}  // namespace

FenValidationResult validate_fen(const std::string& fen) noexcept {
  try {
    std::istringstream input(fen);
    std::vector<std::string> fields;
    std::string field;
    while (input >> field) {
      fields.push_back(field);
    }
    if (fields.size() != 6) {
      return {.error = "FEN must contain exactly six fields"};
    }

    std::array<char, 64> board{};
    int rank = 7;
    int file = 0;
    int white_kings = 0;
    int black_kings = 0;
    for (const char token : fields[0]) {
      if (token == '/') {
        if (file != 8 || rank == 0) {
          return {.error = "Every FEN rank must contain exactly eight squares"};
        }
        --rank;
        file = 0;
        continue;
      }
      if (token >= '1' && token <= '8') {
        file += token - '0';
        if (file > 8) {
          return {.error = "FEN rank contains too many squares"};
        }
        continue;
      }
      if (!is_piece(token) || file >= 8) {
        return {.error = "FEN contains an invalid piece-placement token"};
      }
      if ((rank == 0 || rank == 7) && (token == 'P' || token == 'p')) {
        return {.error = "Pawns cannot be placed on the first or eighth rank"};
      }
      board[static_cast<std::size_t>(rank * 8 + file)] = token;
      white_kings += token == 'K' ? 1 : 0;
      black_kings += token == 'k' ? 1 : 0;
      ++file;
    }
    if (rank != 0 || file != 8) {
      return {.error = "FEN must contain exactly eight ranks"};
    }
    if (white_kings != 1 || black_kings != 1) {
      return {.error = "FEN must contain exactly one king per side"};
    }

    int white_king_square = 0;
    int black_king_square = 0;
    for (int square = 0; square < 64; ++square) {
      if (board[static_cast<std::size_t>(square)] == 'K') white_king_square = square;
      if (board[static_cast<std::size_t>(square)] == 'k') black_king_square = square;
    }
    const int file_distance = std::abs(white_king_square % 8 - black_king_square % 8);
    const int rank_distance = std::abs(white_king_square / 8 - black_king_square / 8);
    if (file_distance <= 1 && rank_distance <= 1) {
      return {.error = "Kings cannot occupy adjacent squares"};
    }

    if (fields[1] != "w" && fields[1] != "b") {
      return {.error = "FEN side-to-move field must be w or b"};
    }

    if (fields[2] != "-") {
      std::set<char> rights;
      for (const char right : fields[2]) {
        if (std::string_view("KQkq").find(right) == std::string_view::npos
            || !rights.insert(right).second) {
          return {.error = "FEN contains invalid or duplicate castling rights"};
        }
      }
      const auto has = [&](const char piece, const char square_file, const char square_rank) {
        return board[static_cast<std::size_t>(square_index(square_file, square_rank))] == piece;
      };
      if ((rights.contains('K') && (!has('K', 'e', '1') || !has('R', 'h', '1')))
          || (rights.contains('Q') && (!has('K', 'e', '1') || !has('R', 'a', '1')))
          || (rights.contains('k') && (!has('k', 'e', '8') || !has('r', 'h', '8')))
          || (rights.contains('q') && (!has('k', 'e', '8') || !has('r', 'a', '8')))) {
        return {.error = "Castling rights require the corresponding king and rook"};
      }
    }

    if (fields[3] != "-") {
      if (fields[3].size() != 2 || fields[3][0] < 'a' || fields[3][0] > 'h'
          || (fields[3][1] != '3' && fields[3][1] != '6')) {
        return {.error = "Invalid en-passant target square"};
      }
      const bool white_to_move = fields[1] == "w";
      if ((white_to_move && fields[3][1] != '6')
          || (!white_to_move && fields[3][1] != '3')) {
        return {.error = "En-passant target is inconsistent with side to move"};
      }
      const int target = square_index(fields[3][0], fields[3][1]);
      if (board[static_cast<std::size_t>(target)] != 0) {
        return {.error = "En-passant target square must be empty"};
      }
      const int pawn_square = target + (white_to_move ? -8 : 8);
      const char expected_pawn = white_to_move ? 'p' : 'P';
      if (board[static_cast<std::size_t>(pawn_square)] != expected_pawn) {
        return {.error = "En-passant target has no corresponding double-pushed pawn"};
      }
    }

    int halfmove = 0;
    int fullmove = 0;
    if (!parse_nonnegative(fields[4], halfmove)) {
      return {.error = "FEN halfmove clock must be a non-negative integer"};
    }
    if (!parse_nonnegative(fields[5], fullmove) || fullmove < 1) {
      return {.error = "FEN fullmove number must be at least one"};
    }

    std::ostringstream normalized;
    for (std::size_t index = 0; index < fields.size(); ++index) {
      if (index != 0) normalized << ' ';
      normalized << fields[index];
    }
    return {.valid = true, .normalized = normalized.str()};
  } catch (const std::exception& error) {
    return {.error = std::string("FEN validation failed: ") + error.what()};
  } catch (...) {
    return {.error = "FEN validation failed"};
  }
}

}  // namespace kchess
