#pragma once

#include <string>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Evidence transport contract
// -----------------------------------------------------------------------------

enum class EvidenceKind {
  cache,
  existing_analysis,
  position_features,
  position_weaknesses,
  weakness_exploitation,
  theory,
  opening,
  user_profile,
  engine,
  conversation,
  tactical_motifs,
  strategic_plans,
  chess_concepts,
  candidate_moves,
  practicality,
};

struct EvidenceItem {
  std::string id;
  EvidenceKind kind{EvidenceKind::cache};
  std::string payload;
  double confidence{1.0};
};

}  // namespace kchess::ai
