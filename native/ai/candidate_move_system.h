#pragma once

#include <optional>
#include <string>

#include "dto/candidate_moves.h"
#include "dto/evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Candidate move selection
// -----------------------------------------------------------------------------

class CandidateMoveSystem {
 public:
  [[nodiscard]] CandidateMoveSet build(
      const CandidateMoveSnapshot& snapshot,
      const std::optional<std::string>& user_move_uci = std::nullopt) const;

  [[nodiscard]] EvidenceItem evidence(const CandidateMoveSet& candidates) const;
};

}  // namespace kchess::ai
