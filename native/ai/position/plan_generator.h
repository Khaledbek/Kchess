#pragma once

#include <vector>

#include "../dto/evidence.h"
#include "exploitation_planner.h"
#include "position_features.h"
#include "weakness_analyzer.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Strategic plan DTOs
// -----------------------------------------------------------------------------

enum class StrategicPlanTheme {
  improve_development,
  improve_worst_piece,
  improve_activity,
  gain_space,
  secure_king,
  pressure_weakness,
  occupy_outpost,
  support_passed_pawn,
};

struct StrategicPlan {
  StrategicPlanTheme theme{StrategicPlanTheme::improve_activity};
  int target_square{-1};
  double priority{0.0};
  double confidence{1.0};
  std::vector<ExploitationMethod> methods;
};

struct PositionPlans {
  std::vector<StrategicPlan> by_white;
  std::vector<StrategicPlan> by_black;
};

// -----------------------------------------------------------------------------
// Section: Feature-to-plan generation
// -----------------------------------------------------------------------------

class PlanGenerator {
 public:
  [[nodiscard]] PositionPlans generate(
      const PositionFeatures& features,
      const PositionWeaknesses& weaknesses,
      const PositionExploitationPlans& exploitation) const;
  [[nodiscard]] EvidenceItem evidence(const PositionPlans& plans) const;
};

}  // namespace kchess::ai
