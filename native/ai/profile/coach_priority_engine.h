#pragma once

#include <vector>

#include "chess_profile.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach goal prioritization
// -----------------------------------------------------------------------------

class CoachPriorityEngine {
 public:
  [[nodiscard]] std::vector<CoachPriority> rank(
      const std::vector<PlayerPattern>& patterns) const;
};

}  // namespace kchess::ai
