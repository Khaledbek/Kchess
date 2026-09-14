#pragma once

#include <cstdint>
#include <vector>

#include "chess_profile.h"
#include "profile_evidence_adapter.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Pattern and strength/weakness interpretation
// -----------------------------------------------------------------------------

struct PatternMatchResult {
  std::vector<PlayerPattern> patterns;
  std::vector<TimeControlProfile> time_control_profiles;
  std::vector<CommonMistake> common_mistakes;
  std::vector<std::string> strengths;
  std::vector<std::string> weaknesses;
  ProfileSignal tactical_strength;
  ProfileSignal calculation_strength;
  ProfileSignal endgame_strength;
  int analyzed_moves{0};
  double confidence{0.0};
};

class PatternMatcher {
 public:
  [[nodiscard]] PatternMatchResult match(
      const std::vector<ProfileGameEvidence>& games,
      std::int64_t now_seconds) const;
};

}  // namespace kchess::ai
