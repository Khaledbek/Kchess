#pragma once

#include <optional>
#include <string>
#include <vector>

#include "candidate_move_system.h"
#include "dto/evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Human/Elo bot evidence contract
// -----------------------------------------------------------------------------

struct HumanMovePrediction {
  std::string candidate_id;
  std::string move_uci;
  int engine_rank{0};
  int loss_cp{0};
  double probability{0.0};
};

struct HumanEloPrediction {
  int requested_elo{0};
  std::vector<HumanMovePrediction> moves;
};

// Reuses the existing KChess bot-strength policy as a human-likelihood model.
// It never decides objective chess truth and never starts an engine search.
// Objective candidate scores must already come from existing analysis/Stockfish.
class HumanEloBotExpert {
 public:
  [[nodiscard]] HumanEloPrediction predict(
      const CandidateMoveSet& candidates,
      int requested_elo) const;

  [[nodiscard]] EvidenceItem evidence(
      const CandidateMoveSet& candidates,
      int requested_elo) const;

  // Produces a compact set of canonical human-strength projections so the LLM
  // can answer practical/human-response questions without the native router
  // having to predict one exact wording or Elo value. If a known learner Elo
  // is supplied it is added to the canonical bands.
  [[nodiscard]] EvidenceItem evidence_grid(
      const CandidateMoveSet& candidates,
      std::optional<int> learner_elo = std::nullopt) const;
};

}  // namespace kchess::ai
