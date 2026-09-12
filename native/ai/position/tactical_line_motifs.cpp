#include "tactical_line_motifs.h"

#include <array>

namespace kchess::ai::detail {
namespace {

// -----------------------------------------------------------------------------
// Section: Board geometry
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

bool supports_direction(Stockfish::PieceType type, const Direction& direction) {
  const bool diagonal = direction.file != 0 && direction.rank != 0;
  if (type == Stockfish::QUEEN) return true;
  if (type == Stockfish::BISHOP) return diagonal;
  return type == Stockfish::ROOK && !diagonal;
}

int tactical_value(Stockfish::PieceType type) {
  switch (type) {
    case Stockfish::KING: return 20000;
    case Stockfish::QUEEN: return 900;
    case Stockfish::ROOK: return 500;
    case Stockfish::BISHOP: return 330;
    case Stockfish::KNIGHT: return 320;
    case Stockfish::PAWN: return 100;
    default: return 0;
  }
}

// -----------------------------------------------------------------------------
// Section: Ray classification
// -----------------------------------------------------------------------------

void classify_enemy_pair(Stockfish::Square attacker_square,
                         Stockfish::Piece first,
                         Stockfish::Square first_square,
                         Stockfish::Piece second,
                         Stockfish::Square second_square,
                         Stockfish::Color attacker,
                         std::vector<TacticalMotif>& motifs) {
  const int first_value = tactical_value(Stockfish::type_of(first));
  const int second_value = tactical_value(Stockfish::type_of(second));

  if (Stockfish::type_of(second) == Stockfish::KING ||
      second_value > first_value) {
    motifs.push_back({
        .kind = TacticalMotifKind::pin,
        .by_white = attacker == Stockfish::WHITE,
        .attacker_squares = {static_cast<int>(attacker_square)},
        .target_squares = {static_cast<int>(first_square),
                           static_cast<int>(second_square)},
        .confidence = Stockfish::type_of(second) == Stockfish::KING ? 1.0 : 0.9,
    });
  }

  if (Stockfish::type_of(first) == Stockfish::KING ||
      first_value > second_value) {
    motifs.push_back({
        .kind = TacticalMotifKind::skewer,
        .by_white = attacker == Stockfish::WHITE,
        .attacker_squares = {static_cast<int>(attacker_square)},
        .target_squares = {static_cast<int>(first_square),
                           static_cast<int>(second_square)},
        .confidence = Stockfish::type_of(first) == Stockfish::KING ? 1.0 : 0.9,
    });
  }
}

void scan_ray(const Stockfish::Position& position,
              Stockfish::Square attacker_square,
              Stockfish::Color attacker,
              const Direction& direction,
              std::vector<TacticalMotif>& motifs) {
  const int start_file = static_cast<int>(Stockfish::file_of(attacker_square));
  const int start_rank = static_cast<int>(Stockfish::rank_of(attacker_square));
  Stockfish::Square first_square = Stockfish::SQ_NONE;
  Stockfish::Piece first = Stockfish::NO_PIECE;

  for (int step = 1; step < 8; ++step) {
    const int file = start_file + direction.file * step;
    const int rank = start_rank + direction.rank * step;
    if (!on_board(file, rank)) break;

    const auto square = square_at(file, rank);
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE) continue;

    if (first == Stockfish::NO_PIECE) {
      first = piece;
      first_square = square;
      continue;
    }

    const bool first_own = Stockfish::color_of(first) == attacker;
    const bool second_enemy = Stockfish::color_of(piece) != attacker;
    if (first_own && second_enemy &&
        Stockfish::type_of(first) != Stockfish::KING) {
      motifs.push_back({
          .kind = TacticalMotifKind::discovered_attack,
          .by_white = attacker == Stockfish::WHITE,
          .attacker_squares = {static_cast<int>(attacker_square)},
          .target_squares = {static_cast<int>(square)},
          .blocker_square = static_cast<int>(first_square),
          .confidence = 0.82,
      });
    } else if (!first_own && second_enemy) {
      classify_enemy_pair(attacker_square, first, first_square, piece, square,
                          attacker, motifs);
    }
    break;
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public line detector
// -----------------------------------------------------------------------------

std::vector<TacticalMotif> detect_line_motifs(
    const Stockfish::Position& position, Stockfish::Color attacker) {
  std::vector<TacticalMotif> motifs;
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != attacker) {
      continue;
    }
    const auto type = Stockfish::type_of(piece);
    if (type != Stockfish::BISHOP && type != Stockfish::ROOK &&
        type != Stockfish::QUEEN) {
      continue;
    }
    for (const auto& direction : kDirections) {
      if (supports_direction(type, direction)) {
        scan_ray(position, square, attacker, direction, motifs);
      }
    }
  }
  return motifs;
}

}  // namespace kchess::ai::detail
