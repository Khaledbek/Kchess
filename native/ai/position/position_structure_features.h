#pragma once

#include <vector>

#include "position.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Strategic position feature DTOs
// -----------------------------------------------------------------------------

struct PawnStructureFeatures {
  int isolated_pawns{0};
  int doubled_pawns{0};
  int connected_pawns{0};
  std::vector<int> passed_pawn_squares;
};

struct KingSafetyFeatures {
  int king_square{-1};
  int pawn_shield{0};
  int exposed_files{0};
  int attacked_zone_squares{0};
  int enemy_attackers{0};
};

struct StrategicSideFeatures {
  PawnStructureFeatures pawns;
  KingSafetyFeatures king;
  std::vector<int> weak_square_candidates;
  std::vector<int> outpost_candidates;
  std::vector<int> bad_piece_candidates;
  std::vector<int> bad_knight_candidates;
  std::vector<int> bad_bishop_candidates;
  int light_square_pawns{0};
  int dark_square_pawns{0};
  int light_square_bishops{0};
  int dark_square_bishops{0};
};

// -----------------------------------------------------------------------------
// Section: Deterministic strategic extraction
// -----------------------------------------------------------------------------

[[nodiscard]] StrategicSideFeatures extract_strategic_features(
    const Stockfish::Position& position, Stockfish::Color color);

}  // namespace kchess::ai
