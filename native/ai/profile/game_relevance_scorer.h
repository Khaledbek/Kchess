#pragma once

#include <cstdint>
#include <string>

#include "profile_evidence_adapter.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Initial-profile relevance scoring
// -----------------------------------------------------------------------------

struct GameRelevanceScore {
  std::string game_id;
  double score{0.0};
  double recency{0.0};
  double analysis_value{0.0};
  double error_signal{0.0};
  double diversity_value{0.0};
};

class GameRelevanceScorer {
 public:
  [[nodiscard]] GameRelevanceScore score(
      const ProfileGameEvidence& game,
      std::int64_t now_seconds) const;
};

}  // namespace kchess::ai
