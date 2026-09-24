#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "chess_profile.h"
#include "profile_evidence_adapter.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Profile hypotheses
// -----------------------------------------------------------------------------

class HypothesisManager {
 public:
  [[nodiscard]] std::vector<ProfileHypothesis> update(
      const std::vector<PlayerPattern>& patterns,
      const std::vector<ProfileGameEvidence>& games,
      std::int64_t now_seconds,
      const std::optional<ChessProfile>& previous = std::nullopt) const;
};

}  // namespace kchess::ai
