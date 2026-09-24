#pragma once

#include <vector>

#include "position.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Raw weakness-oriented board facts
// -----------------------------------------------------------------------------

struct PositionWeaknessFacts {
  std::vector<int> backward_pawn_candidates;
  std::vector<int> loose_piece_candidates;
  std::vector<int> unprotected_pawn_candidates;
  std::vector<int> overloaded_defender_candidates;
  bool weak_back_rank_candidate{false};
};

// -----------------------------------------------------------------------------
// Section: Deterministic extraction
// -----------------------------------------------------------------------------

[[nodiscard]] PositionWeaknessFacts extract_position_weakness_facts(
    const Stockfish::Position& position, Stockfish::Color color);

}  // namespace kchess::ai
