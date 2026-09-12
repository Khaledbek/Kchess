#include "practicality_engine.h"

#include <utility>

#include <nlohmann/json.hpp>

#include "practicality_metrics.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Evidence serialization
// -----------------------------------------------------------------------------

template <typename T>
void put_optional(nlohmann::json& target, const char* key,
                  const std::optional<T>& value) {
  if (value) target[key] = *value;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public practicality assessment
// -----------------------------------------------------------------------------

PracticalityAssessment PracticalityEngine::assess(
    const CandidateMoveSet& candidates,
    const std::optional<PositionFeatures>& features,
    const std::optional<TacticalAnalysis>& tactics) const {
  return detail::build_objective_assessment(candidates, features, tactics);
}

EvidenceItem PracticalityEngine::evidence(
    const PracticalityAssessment& assessment) const {
  nlohmann::json metrics{
      {"criticalReplyCount", assessment.metrics.critical_reply_count},
      {"forcedLineLength", assessment.metrics.forced_line_length},
  };
  put_optional(metrics, "evaluationLossIfInaccurateCp",
               assessment.metrics.evaluation_loss_if_inaccurate_cp);
  put_optional(metrics, "onlyMoveDensity", assessment.metrics.only_move_density);
  put_optional(metrics, "evaluationVolatility",
               assessment.metrics.evaluation_volatility);
  put_optional(metrics, "branchingComplexity",
               assessment.metrics.branching_complexity);
  put_optional(metrics, "kingExposure", assessment.metrics.king_exposure);
  put_optional(metrics, "calculationDepthRequired",
               assessment.metrics.calculation_depth_required);
  put_optional(metrics, "tacticalDensity", assessment.metrics.tactical_density);
  put_optional(metrics, "positionStability",
               assessment.metrics.position_stability);

  nlohmann::json payload{
      {"version", 1},
      {"metrics", std::move(metrics)},
      {"playerAdjusted", false},
  };
  put_optional(payload, "difficulty", assessment.difficulty);
  put_optional(payload, "risk", assessment.risk);
  put_optional(payload, "forgiveness", assessment.forgiveness);
  put_optional(payload, "clarity", assessment.clarity);
  return {
      .id = "practicality.objective.v1",
      .kind = EvidenceKind::practicality,
      .payload = payload.dump(),
      .confidence = assessment.confidence,
  };
}

}  // namespace kchess::ai
