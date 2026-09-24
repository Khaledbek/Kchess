#include "tactical_pattern_motifs.h"

#include <algorithm>
#include <array>
#include <utility>

#include "bitboard.h"

namespace kchess::ai::detail {
namespace {

// -----------------------------------------------------------------------------
// Section: Geometry helpers
// -----------------------------------------------------------------------------

struct Direction {
  int file;
  int rank;
};

constexpr std::array<Direction, 8> kDirections{{
    {1, 0}, {-1, 0}, {0, 1}, {0, -1},
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
}};

bool on_board(int file, int rank) {
  return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

Stockfish::Square square_at(int file, int rank) {
  return static_cast<Stockfish::Square>(rank * 8 + file);
}

Stockfish::Bitboard square_bit(Stockfish::Square square) {
  return Stockfish::Bitboard{1} << static_cast<unsigned>(square);
}

bool supports(Stockfish::PieceType type, const Direction& direction) {
  const bool diagonal = direction.file != 0 && direction.rank != 0;
  return type == Stockfish::QUEEN ||
         (diagonal && type == Stockfish::BISHOP) ||
         (!diagonal && type == Stockfish::ROOK);
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

void append_squares(Stockfish::Bitboard board, std::vector<int>& output) {
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (board & square_bit(square)) output.push_back(static_cast<int>(square));
  }
}

// -----------------------------------------------------------------------------
// Section: X-ray and battery
// -----------------------------------------------------------------------------

void scan_slider_ray(const Stockfish::Position& position,
                     Stockfish::Square rear_square,
                     Stockfish::Color attacker,
                     const Direction& direction,
                     std::vector<TacticalMotif>& motifs) {
  const int start_file = static_cast<int>(Stockfish::file_of(rear_square));
  const int start_rank = static_cast<int>(Stockfish::rank_of(rear_square));
  Stockfish::Square first_square = Stockfish::SQ_NONE;
  Stockfish::Piece first_piece = Stockfish::NO_PIECE;

  for (int step = 1; step < 8; ++step) {
    const int file = start_file + direction.file * step;
    const int rank = start_rank + direction.rank * step;
    if (!on_board(file, rank)) break;
    const auto square = square_at(file, rank);
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE) continue;

    if (first_piece == Stockfish::NO_PIECE) {
      first_square = square;
      first_piece = piece;
      continue;
    }

    if (Stockfish::color_of(piece) != attacker &&
        Stockfish::type_of(piece) != Stockfish::PAWN) {
      const bool front_is_battery =
          Stockfish::color_of(first_piece) == attacker &&
          supports(Stockfish::type_of(first_piece), direction);
      motifs.push_back({
          .kind = front_is_battery ? TacticalMotifKind::battery
                                   : TacticalMotifKind::x_ray,
          .by_white = attacker == Stockfish::WHITE,
          .attacker_squares = front_is_battery
                                  ? std::vector<int>{static_cast<int>(rear_square),
                                                     static_cast<int>(first_square)}
                                  : std::vector<int>{static_cast<int>(rear_square)},
          .target_squares = {static_cast<int>(square)},
          .blocker_square = front_is_battery ? -1 : static_cast<int>(first_square),
          .confidence = front_is_battery ? 0.9 : 0.8,
      });
    }
    break;
  }
}

void detect_slider_patterns(const Stockfish::Position& position,
                            Stockfish::Color attacker,
                            std::vector<TacticalMotif>& motifs) {
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != attacker) {
      continue;
    }
    const auto type = Stockfish::type_of(piece);
    for (const auto& direction : kDirections) {
      if (supports(type, direction)) {
        scan_slider_ray(position, square, attacker, direction, motifs);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Section: Overloading
// -----------------------------------------------------------------------------

void detect_overloading(const Stockfish::Position& position,
                        Stockfish::Color defender_color,
                        std::vector<TacticalMotif>& motifs) {
  const auto enemy = position.pieces(~defender_color);

  for (Stockfish::Square defender = Stockfish::SQ_A1;
       defender <= Stockfish::SQ_H8; ++defender) {
    const auto defender_piece = position.piece_on(defender);
    if (defender_piece == Stockfish::NO_PIECE ||
        Stockfish::color_of(defender_piece) != defender_color ||
        Stockfish::type_of(defender_piece) == Stockfish::KING) {
      continue;
    }

    std::vector<int> targets;
    std::vector<int> attackers;
    for (Stockfish::Square target = Stockfish::SQ_A1;
         target <= Stockfish::SQ_H8; ++target) {
      const auto target_piece = position.piece_on(target);
      if (target_piece == Stockfish::NO_PIECE ||
          Stockfish::color_of(target_piece) != defender_color ||
          Stockfish::type_of(target_piece) == Stockfish::KING) {
        continue;
      }
      const auto all_attackers = position.attackers_to(target);
      if (!(all_attackers & square_bit(defender)) || !(all_attackers & enemy)) {
        continue;
      }
      targets.push_back(static_cast<int>(target));
      append_squares(all_attackers & enemy, attackers);
    }

    if (targets.size() < 2) continue;
    std::sort(attackers.begin(), attackers.end());
    attackers.erase(std::unique(attackers.begin(), attackers.end()), attackers.end());
    motifs.push_back({
        .kind = TacticalMotifKind::overloading,
        .by_white = defender_color == Stockfish::BLACK,
        .attacker_squares = std::move(attackers),
        .target_squares = std::move(targets),
        .blocker_square = static_cast<int>(defender),
        .confidence = 0.86,
    });
  }
}

// -----------------------------------------------------------------------------
// Section: Back-rank pressure
// -----------------------------------------------------------------------------

bool boxed_king(const Stockfish::Position& position, Stockfish::Color color,
                Stockfish::Square king) {
  const auto own = position.pieces(color);
  const auto enemy = position.pieces(~color);
  const int file = static_cast<int>(Stockfish::file_of(king));
  const int rank = static_cast<int>(Stockfish::rank_of(king));
  for (int df = -1; df <= 1; ++df) {
    for (int dr = -1; dr <= 1; ++dr) {
      if (df == 0 && dr == 0) continue;
      const int next_file = file + df;
      const int next_rank = rank + dr;
      if (!on_board(next_file, next_rank)) continue;
      const auto square = square_at(next_file, next_rank);
      if (own & square_bit(square)) continue;
      if (position.attackers_to(square) & enemy) continue;
      return false;
    }
  }
  return true;
}


bool back_rank_slider_candidate(const Stockfish::Position& position,
                                Stockfish::Color victim,
                                Stockfish::Square king) {
  const int king_file = static_cast<int>(Stockfish::file_of(king));
  const int king_rank = static_cast<int>(Stockfish::rank_of(king));
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) == victim) {
      continue;
    }
    const auto type = Stockfish::type_of(piece);
    if (type != Stockfish::ROOK && type != Stockfish::QUEEN) continue;
    if (static_cast<int>(Stockfish::rank_of(square)) != king_rank) continue;

    const int slider_file = static_cast<int>(Stockfish::file_of(square));
    const int step = slider_file < king_file ? 1 : -1;
    int blockers = 0;
    for (int file = slider_file + step; file != king_file; file += step) {
      if (position.piece_on(square_at(file, king_rank)) != Stockfish::NO_PIECE) {
        ++blockers;
      }
    }
    if (blockers <= 1) return true;
  }
  return false;
}

void detect_back_rank(const Stockfish::Position& position,
                      Stockfish::Color victim,
                      std::vector<TacticalMotif>& motifs) {
  const auto king = king_square(position, victim);
  if (king == Stockfish::SQ_NONE) return;
  const int home_rank = victim == Stockfish::WHITE ? 0 : 7;
  if (static_cast<int>(Stockfish::rank_of(king)) != home_rank ||
      !boxed_king(position, victim, king)) {
    return;
  }

  std::vector<int> attackers;
  const auto pressure = position.attackers_to(king) & position.pieces(~victim);
  append_squares(pressure, attackers);
  if (attackers.empty() && !back_rank_slider_candidate(position, victim, king)) {
    return;
  }

  motifs.push_back({
      .kind = TacticalMotifKind::back_rank,
      .by_white = victim == Stockfish::BLACK,
      .attacker_squares = std::move(attackers),
      .target_squares = {static_cast<int>(king)},
      .confidence = pressure ? 0.96 : 0.72,
  });
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public static detector
// -----------------------------------------------------------------------------

std::vector<TacticalMotif> detect_pattern_motifs(
    const Stockfish::Position& position) {
  std::vector<TacticalMotif> motifs;
  for (const auto color : {Stockfish::WHITE, Stockfish::BLACK}) {
    detect_slider_patterns(position, color, motifs);
    detect_overloading(position, color, motifs);
    detect_back_rank(position, color, motifs);
  }
  return motifs;
}

}  // namespace kchess::ai::detail
