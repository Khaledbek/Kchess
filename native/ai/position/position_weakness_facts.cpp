#include "position_weakness_facts.h"

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

bool on_board(int file, int rank) {
  return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

Stockfish::Square square_at(int file, int rank) {
  return static_cast<Stockfish::Square>(rank * 8 + file);
}

Stockfish::Bitboard square_bit(Stockfish::Square square) {
  return Stockfish::Bitboard{1} << static_cast<unsigned>(square);
}

int relative_rank(Stockfish::Square square, Stockfish::Color color) {
  const int rank = square_rank(square);
  return color == Stockfish::WHITE ? rank : 7 - rank;
}

Stockfish::Square king_square(const Stockfish::Position& position,
                              Stockfish::Color color) {
  const auto king = Stockfish::make_piece(color, Stockfish::KING);
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (position.piece_on(square) == king) return square;
  }
  return Stockfish::SQ_NONE;
}

// -----------------------------------------------------------------------------
// Section: Pawn and protection facts
// -----------------------------------------------------------------------------

bool enemy_pawn_attacks(const Stockfish::Position& position,
                        Stockfish::Square square, Stockfish::Color color) {
  return (position.attackers_to(square) &
          position.pieces(~color, Stockfish::PAWN)) != 0;
}

bool has_adjacent_pawn_ahead(const Stockfish::Position& position,
                             Stockfish::Square square,
                             Stockfish::Color color) {
  const auto pawn = Stockfish::make_piece(color, Stockfish::PAWN);
  for (Stockfish::Square other = Stockfish::SQ_A1;
       other <= Stockfish::SQ_H8; ++other) {
    if (position.piece_on(other) != pawn) continue;
    if (std::abs(square_file(other) - square_file(square)) != 1) continue;
    if (relative_rank(other, color) > relative_rank(square, color)) return true;
  }
  return false;
}

bool has_adjacent_pawn_same_rank(const Stockfish::Position& position,
                                 Stockfish::Square square,
                                 Stockfish::Color color) {
  const auto pawn = Stockfish::make_piece(color, Stockfish::PAWN);
  for (Stockfish::Square other = Stockfish::SQ_A1;
       other <= Stockfish::SQ_H8; ++other) {
    if (position.piece_on(other) != pawn) continue;
    if (std::abs(square_file(other) - square_file(square)) == 1 &&
        square_rank(other) == square_rank(square)) {
      return true;
    }
  }
  return false;
}

std::vector<int> backward_pawn_candidates(const Stockfish::Position& position,
                                          Stockfish::Color color) {
  std::vector<int> result;
  const auto pawn = Stockfish::make_piece(color, Stockfish::PAWN);
  const int advance = color == Stockfish::WHITE ? 1 : -1;

  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (position.piece_on(square) != pawn) continue;
    const int next_rank = square_rank(square) + advance;
    if (!on_board(square_file(square), next_rank)) continue;
    const auto forward = square_at(square_file(square), next_rank);
    if (position.piece_on(forward) != Stockfish::NO_PIECE) continue;
    if (!has_adjacent_pawn_ahead(position, square, color)) continue;
    if (has_adjacent_pawn_same_rank(position, square, color)) continue;
    if (enemy_pawn_attacks(position, forward, color)) {
      result.push_back(static_cast<int>(square));
    }
  }
  return result;
}

void collect_unprotected(const Stockfish::Position& position,
                         Stockfish::Color color, PositionWeaknessFacts& result) {
  const auto own = position.pieces(color);
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != color) {
      continue;
    }
    const auto type = Stockfish::type_of(piece);
    if (type == Stockfish::KING) continue;
    if (position.attackers_to(square) & own) continue;

    if (type == Stockfish::PAWN) {
      result.unprotected_pawn_candidates.push_back(static_cast<int>(square));
    } else {
      result.loose_piece_candidates.push_back(static_cast<int>(square));
    }
  }
}

// -----------------------------------------------------------------------------
// Section: Defender load and back rank
// -----------------------------------------------------------------------------

std::vector<int> overloaded_defender_candidates(
    const Stockfish::Position& position, Stockfish::Color color) {
  std::array<int, 64> burden{};
  const auto own = position.pieces(color);
  const auto enemy = position.pieces(~color);

  for (Stockfish::Square target = Stockfish::SQ_A1;
       target <= Stockfish::SQ_H8; ++target) {
    const auto piece = position.piece_on(target);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != color ||
        Stockfish::type_of(piece) == Stockfish::KING) {
      continue;
    }
    if (!(position.attackers_to(target) & enemy)) continue;

    const auto defenders = position.attackers_to(target) & own;
    if (Stockfish::popcount(defenders) != 1) continue;
    for (Stockfish::Square defender = Stockfish::SQ_A1;
         defender <= Stockfish::SQ_H8; ++defender) {
      if (defenders & square_bit(defender)) {
        ++burden[static_cast<int>(defender)];
        break;
      }
    }
  }

  std::vector<int> result;
  for (int square = 0; square < 64; ++square) {
    if (burden[square] >= 2) result.push_back(square);
  }
  return result;
}

bool weak_back_rank_candidate(const Stockfish::Position& position,
                              Stockfish::Color color) {
  const auto king = king_square(position, color);
  if (king == Stockfish::SQ_NONE) return false;
  const int home_rank = color == Stockfish::WHITE ? 0 : 7;
  if (square_rank(king) != home_rank) return false;
  if (position.count<Stockfish::ROOK>(~color) == 0 &&
      position.count<Stockfish::QUEEN>(~color) == 0) {
    return false;
  }

  const auto own = position.pieces(color);
  const auto enemy = position.pieces(~color);
  for (int df = -1; df <= 1; ++df) {
    for (int dr = -1; dr <= 1; ++dr) {
      if (df == 0 && dr == 0) continue;
      const int file = square_file(king) + df;
      const int rank = square_rank(king) + dr;
      if (!on_board(file, rank)) continue;
      const auto destination = square_at(file, rank);
      if (own & square_bit(destination)) continue;
      if (!(position.attackers_to(destination) & enemy)) return false;
    }
  }
  return true;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public extraction
// -----------------------------------------------------------------------------

PositionWeaknessFacts extract_position_weakness_facts(
    const Stockfish::Position& position, Stockfish::Color color) {
  PositionWeaknessFacts result;
  result.backward_pawn_candidates = backward_pawn_candidates(position, color);
  collect_unprotected(position, color, result);
  result.overloaded_defender_candidates =
      overloaded_defender_candidates(position, color);
  result.weak_back_rank_candidate = weak_back_rank_candidate(position, color);
  return result;
}

}  // namespace kchess::ai
