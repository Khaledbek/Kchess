#pragma once

#include <string>
#include <vector>

#include "../coach_types.h"
#include "../dto/coach_request.h"
#include "../dto/evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Stable native teaching-skill identity
// -----------------------------------------------------------------------------

struct TeachingSkill {
  std::string id{"calculation.candidate_selection"};
  std::string family{"calculation"};
};

// Resolves one bounded skill identity from already-available native evidence.
// It never starts analysis and never turns an unverified heuristic into a
// measured player ability. The identity is used only to group verified practice
// attempts and future scheduling decisions.
[[nodiscard]] TeachingSkill resolve_teaching_skill(
    const CoachRequest& request, CoachIntent topic,
    const std::vector<EvidenceItem>& evidence);

}  // namespace kchess::ai
