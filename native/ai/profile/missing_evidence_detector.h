#pragma once

#include <optional>
#include <string>

#include "profile_evidence_adapter.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Missing-evidence policy
// -----------------------------------------------------------------------------

enum class MissingEvidenceAction {
  none,
  quick_probe,
  deep_probe,
};

struct MissingEvidenceDecision {
  MissingEvidenceAction action{MissingEvidenceAction::none};
  std::optional<int> ply;
  std::string reason;
};

class MissingEvidenceDetector {
 public:
  [[nodiscard]] MissingEvidenceDecision inspect(
      const ProfileGameEvidence& game,
      double relevance_score) const;
};

}  // namespace kchess::ai
