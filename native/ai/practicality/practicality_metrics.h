#pragma once

#include "practicality_engine.h"

namespace kchess::ai::detail {

[[nodiscard]] PracticalityAssessment build_objective_assessment(
    const CandidateMoveSet& candidates,
    const std::optional<PositionFeatures>& features,
    const std::optional<TacticalAnalysis>& tactics);

}  // namespace kchess::ai::detail
