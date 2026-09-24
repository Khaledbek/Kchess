#pragma once

#include <cstdint>
#include <vector>

#include "chess_profile.h"
#include "profile_evidence_adapter.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Recency-aware trend detection
// -----------------------------------------------------------------------------

class ProfileTrendEngine {
 public:
  void apply(
      std::vector<PlayerPattern>& patterns,
      std::vector<TimeControlProfile>& time_control_profiles,
      const std::vector<ProfileGameEvidence>& games,
      std::int64_t now_seconds) const;
};

}  // namespace kchess::ai
