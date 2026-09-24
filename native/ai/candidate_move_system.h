#pragma once

#include <optional>
#include <string>

#include "coach_types.h"
#include "dto/candidate_moves.h"
#include "dto/chess_facts.h"
#include "dto/evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Candidate move selection
// -----------------------------------------------------------------------------

class CandidateMoveSystem {
 public:
  [[nodiscard]] CandidateMoveSet build(
      const CandidateMoveSnapshot& snapshot,
      const std::optional<std::string>& user_move_uci = std::nullopt,
      PositionAnalysisMode analysis_mode = PositionAnalysisMode::none) const;

  [[nodiscard]] std::vector<ChessFact> facts(
      const CandidateMoveSet& candidates) const;

  [[nodiscard]] EvidenceItem evidence(const CandidateMoveSet& candidates) const;
};

}  // namespace kchess::ai
