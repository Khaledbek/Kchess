#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../dto/candidate_moves.h"
#include "../dto/evidence.h"
#include "practicality_engine.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Minimal player context for practical move choice
// -----------------------------------------------------------------------------

struct PracticalityPlayerContext {
  std::optional<int> rating;
  std::optional<double> calculation_strength;
  std::optional<double> tactical_strength;
  std::optional<double> risk_tolerance;
  double confidence{0.0};
};

struct PracticalCandidateFit {
  std::string move_uci;
  int engine_rank{0};
  std::optional<int> evaluation_loss_cp;
  bool objectively_eligible{false};
  std::optional<double> difficulty;
  std::optional<double> risk;
  std::optional<double> fit;
};

struct PlayerPracticalityAssessment {
  PracticalityPlayerContext player;
  std::vector<PracticalCandidateFit> candidates;
  std::optional<std::string> practical_choice_uci;
  double confidence{0.0};
};

// -----------------------------------------------------------------------------
// Section: Player-aware practicality overlay
// -----------------------------------------------------------------------------

class PlayerPracticalityEngine {
 public:
  [[nodiscard]] PlayerPracticalityAssessment assess(
      const CandidateMoveSet& candidates,
      const PracticalityAssessment& objective,
      const PracticalityPlayerContext& player) const;

  [[nodiscard]] EvidenceItem evidence(
      const PlayerPracticalityAssessment& assessment) const;
};

}  // namespace kchess::ai
