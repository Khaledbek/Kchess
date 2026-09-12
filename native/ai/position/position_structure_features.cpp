#include "position_structure_features.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "bitboard.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Square helpers
// -----------------------------------------------------------------------------

int square_file(Stockfish::Square square) {
  return static_cast<int>(Stockfish::file_of(square));
}

int square_rank(Stockfish::Square square) {
  return static_cast<int>(Stockfish::rank_of(square));
}

Stockfish::Square square_at(int file, int rank) {
  return static_cast<Stockfish::Square>(rank * 8 + file);
}

bool on_board(int file, int rank) {
  return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

bool light_square(Stockfish::Square square) {
  return ((square_file(square) + square_rank(square)) & 1) != 0;
}

int relative_rank(Stockfish::Square square, Stockfish::Color color) {
  const int rank = square_rank(square);
  return color == Stockfish::WHITE ? rank : 7 - rank;
}

bool pawn_ahead(Stockfish::Square pawn, Stockfish::Square target,
                Stockfish::Color color) {
  return color == Stockfish::WHITE ? square_rank(pawn) > square_rank(target)
                                   : square_rank(pawn) < square_rank(target);
}

// -----------------------------------------------------------------------------
// Section: Pawn structure
// -----------------------------------------------------------------------------

bool has_adjacent_pawn(const Stockfish::Position& position,
                       Stockfish::Square square, Stockfish::Color color,
                       bool require_near_rank) {
  for (Stockfish::Square other = Stockfish::SQ_A1;
       other <= Stockfish::SQ_H8; ++other) {
    if (position.piece_on(other) != Stockfish::make_piece(color, Stockfish::PAWN)) {
      continue;
    }
    if (std::abs(square_file(other) - square_file(square)) != 1) continue;
    if (!require_near_rank || std::abs(square_rank(other) - square_rank(square)) <= 1) {
      return true;
    }
  }
  return false;
}

bool passed_pawn(const Stockfish::Position& position, Stockfish::Square square,
                 Stockfish::Color color) {
  const auto enemy_pawn = Stockfish::make_piece(~color, Stockfish::PAWN);
  for (Stockfish::Square other = Stockfish::SQ_A1;
       other <= Stockfish::SQ_H8; ++other) {
    if (position.piece_on(other) != enemy_pawn) continue;
    if (std::abs(square_file(other) - square_file(square)) > 1) continue;
    if (pawn_ahead(other, square, color)) return false;
  }
  return true;
}

PawnStructureFeatures pawn_features(const Stockfish::Position& position,
                                    Stockfish::Color color) {
  PawnStructureFeatures result;
  std::array<int, 8> pawns_per_file{};
  const auto pawn = Stockfish::make_piece(color, Stockfish::PAWN);

  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (position.piece_on(square) != pawn) continue;
    ++pawns_per_file[square_file(square)];
    if (!has_adjacent_pawn(position, square, color, false)) {
      ++result.isolated_pawns;
    }
    if (has_adjacent_pawn(position, square, color, true)) {
      ++result.connected_pawns;
    }
    if (passed_pawn(position, square, color)) {
      result.passed_pawn_squares.push_back(static_cast<int>(square));
    }
  }

  for (const int count : pawns_per_file) {
    if (count > 1) result.doubled_pawns += count - 1;
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Squares and outposts
// -----------------------------------------------------------------------------

bool enemy_pawn_can_challenge(const Stockfish::Position& position,
                              Stockfish::Square square,
                              Stockfish::Color color) {
  const auto enemy_pawn = Stockfish::make_piece(~color, Stockfish::PAWN);
  for (Stockfish::Square pawn = Stockfish::SQ_A1;
       pawn <= Stockfish::SQ_H8; ++pawn) {
    if (position.piece_on(pawn) != enemy_pawn) continue;
    if (std::abs(square_file(pawn) - square_file(square)) != 1) continue;
    if (pawn_ahead(pawn, square, color)) return true;
  }
  return false;
}

std::vector<int> weak_square_candidates(const Stockfish::Position& position,
                                        Stockfish::Color color) {
  std::vector<int> result;
  const auto own_pawns = position.pieces(color, Stockfish::PAWN);
  const auto enemy = position.pieces(~color);

  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (relative_rank(square, color) > 3) continue;
    if (position.piece_on(square) == Stockfish::make_piece(color, Stockfish::PAWN)) {
      continue;
    }
    const auto attackers = position.attackers_to(square);
    if ((attackers & enemy) && !(attackers & own_pawns)) {
      result.push_back(static_cast<int>(square));
    }
  }
  return result;
}

std::vector<int> outpost_candidates(const Stockfish::Position& position,
                                    Stockfish::Color color) {
  std::vector<int> result;
  const auto own_pawns = position.pieces(color, Stockfish::PAWN);
  const auto enemy_pawns = position.pieces(~color, Stockfish::PAWN);

  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const int rel_rank = relative_rank(square, color);
    if (rel_rank < 3 || rel_rank > 5) continue;
    if (position.piece_on(square) == Stockfish::make_piece(color, Stockfish::PAWN)) {
      continue;
    }
    const auto attackers = position.attackers_to(square);
    if (!(attackers & own_pawns) || (attackers & enemy_pawns)) continue;
    if (enemy_pawn_can_challenge(position, square, color)) continue;
    result.push_back(static_cast<int>(square));
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Piece quality
// -----------------------------------------------------------------------------

int knight_mobility(const Stockfish::Position& position,
                    Stockfish::Square square, Stockfish::Color color) {
  static constexpr std::array<std::array<int, 2>, 8> kOffsets{{
      {{1, 2}}, {{2, 1}}, {{2, -1}}, {{1, -2}},
      {{-1, -2}}, {{-2, -1}}, {{-2, 1}}, {{-1, 2}},
  }};
  int count = 0;
  for (const auto& offset : kOffsets) {
    const int file = square_file(square) + offset[0];
    const int rank = square_rank(square) + offset[1];
    if (!on_board(file, rank)) continue;
    const auto piece = position.piece_on(square_at(file, rank));
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != color) {
      ++count;
    }
  }
  return count;
}

int bishop_mobility(const Stockfish::Position& position,
                    Stockfish::Square square, Stockfish::Color color) {
  static constexpr std::array<std::array<int, 2>, 4> kDirections{{
      {{1, 1}}, {{1, -1}}, {{-1, 1}}, {{-1, -1}},
  }};
  int count = 0;
  for (const auto& direction : kDirections) {
    int file = square_file(square) + direction[0];
    int rank = square_rank(square) + direction[1];
    while (on_board(file, rank)) {
      const auto piece = position.piece_on(square_at(file, rank));
      if (piece == Stockfish::NO_PIECE) {
        ++count;
      } else {
        if (Stockfish::color_of(piece) != color) ++count;
        break;
      }
      file += direction[0];
      rank += direction[1];
    }
  }
  return count;
}

void collect_bad_piece_candidates(const Stockfish::Position& position,
                                  Stockfish::Color color,
                                  StrategicSideFeatures& result) {
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != color) {
      continue;
    }
    const auto type = Stockfish::type_of(piece);
    const int index = static_cast<int>(square);
    if (type == Stockfish::KNIGHT && knight_mobility(position, square, color) <= 2) {
      result.bad_piece_candidates.push_back(index);
      result.bad_knight_candidates.push_back(index);
    } else if (type == Stockfish::BISHOP &&
               bishop_mobility(position, square, color) <= 3) {
      result.bad_piece_candidates.push_back(index);
      result.bad_bishop_candidates.push_back(index);
    }
  }
}

// -----------------------------------------------------------------------------
// Section: King safety and color complexes
// -----------------------------------------------------------------------------

Stockfish::Square king_square(const Stockfish::Position& position,
                              Stockfish::Color color) {
  const auto king = Stockfish::make_piece(color, Stockfish::KING);
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (position.piece_on(square) == king) return square;
  }
  return Stockfish::SQ_NONE;
}

KingSafetyFeatures king_features(const Stockfish::Position& position,
                                 Stockfish::Color color) {
  KingSafetyFeatures result;
  const auto king = king_square(position, color);
  if (king == Stockfish::SQ_NONE) return result;

  result.king_square = static_cast<int>(king);
  const auto enemy = position.pieces(~color);
  Stockfish::Bitboard attackers = 0;
  const int advance = color == Stockfish::WHITE ? 1 : -1;
  const int king_file = square_file(king);
  const int king_rank = square_rank(king);

  for (int df = -1; df <= 1; ++df) {
    const int file = king_file + df;
    if (file < 0 || file >= 8) continue;

    bool own_pawn_on_file = false;
    for (Stockfish::Square square = Stockfish::SQ_A1;
         square <= Stockfish::SQ_H8; ++square) {
      if (square_file(square) == file &&
          position.piece_on(square) == Stockfish::make_piece(color, Stockfish::PAWN)) {
        own_pawn_on_file = true;
        break;
      }
    }
    if (!own_pawn_on_file) ++result.exposed_files;

    const int shield_rank = king_rank + advance;
    if (on_board(file, shield_rank) &&
        position.piece_on(square_at(file, shield_rank)) ==
            Stockfish::make_piece(color, Stockfish::PAWN)) {
      ++result.pawn_shield;
    }
  }

  for (int df = -1; df <= 1; ++df) {
    for (int dr = -1; dr <= 1; ++dr) {
      const int file = king_file + df;
      const int rank = king_rank + dr;
      if (!on_board(file, rank)) continue;
      const auto square = square_at(file, rank);
      const auto square_attackers = position.attackers_to(square) & enemy;
      if (square_attackers) ++result.attacked_zone_squares;
      attackers |= square_attackers;
    }
  }
  result.enemy_attackers = Stockfish::popcount(attackers);
  return result;
}

void color_complexes(const Stockfish::Position& position, Stockfish::Color color,
                     StrategicSideFeatures& result) {
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != color) {
      continue;
    }
    const bool light = light_square(square);
    if (Stockfish::type_of(piece) == Stockfish::PAWN) {
      light ? ++result.light_square_pawns : ++result.dark_square_pawns;
    } else if (Stockfish::type_of(piece) == Stockfish::BISHOP) {
      light ? ++result.light_square_bishops : ++result.dark_square_bishops;
    }
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public extraction
// -----------------------------------------------------------------------------

StrategicSideFeatures extract_strategic_features(
    const Stockfish::Position& position, Stockfish::Color color) {
  StrategicSideFeatures result;
  result.pawns = pawn_features(position, color);
  result.king = king_features(position, color);
  result.weak_square_candidates = weak_square_candidates(position, color);
  result.outpost_candidates = outpost_candidates(position, color);
  collect_bad_piece_candidates(position, color, result);
  color_complexes(position, color, result);
  return result;
}

}  // namespace kchess::ai
