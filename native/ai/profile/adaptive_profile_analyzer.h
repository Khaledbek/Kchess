#pragma once

#include <optional>
#include <string>

#include "missing_evidence_detector.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Engine-work request contract
// -----------------------------------------------------------------------------

enum class ProfileAnalysisAction {
  none,
  quick_game_analysis,
  deep_move_refinement,
};

struct ProfileAnalysisRequest {
  ProfileAnalysisAction action{ProfileAnalysisAction::none};
  std::string game_id;
  std::optional<int> ply;
  std::string reason;
};

class AdaptiveProfileAnalyzer {
 public:
  [[nodiscard]] ProfileAnalysisRequest plan(
      const ProfileGameEvidence& game,
      double relevance_score) const;
};

}  // namespace kchess::ai
