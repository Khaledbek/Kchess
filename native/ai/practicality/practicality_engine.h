#pragma once

#include <optional>

#include "../dto/candidate_moves.h"
#include "../dto/evidence.h"
#include "../position/position_features.h"
#include "../position/tactical_detector.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Objective practicality metrics
// -----------------------------------------------------------------------------

struct PracticalityMetrics {
  std::optional<int> evaluation_loss_if_inaccurate_cp;
  std::optional<double> only_move_density;
  std::optional<double> evaluation_volatility;
  int critical_reply_count{0};
  std::optional<double> branching_complexity;
  int forced_line_length{0};
  std::optional<double> king_exposure;
  std::optional<double> calculation_depth_required;
  std::optional<double> tactical_density;
  std::optional<double> position_stability;
};

struct PracticalityAssessment {
  PracticalityMetrics metrics;
  std::optional<double> difficulty;
  std::optional<double> risk;
  std::optional<double> forgiveness;
  std::optional<double> clarity;
  double confidence{0.0};
};

// -----------------------------------------------------------------------------
// Section: Engine-independent practicality assessment
// -----------------------------------------------------------------------------

class PracticalityEngine {
 public:
  [[nodiscard]] PracticalityAssessment assess(
      const CandidateMoveSet& candidates,
      const std::optional<PositionFeatures>& features = std::nullopt,
      const std::optional<TacticalAnalysis>& tactics = std::nullopt) const;

  [[nodiscard]] EvidenceItem evidence(
      const PracticalityAssessment& assessment) const;
};

}  // namespace kchess::ai
