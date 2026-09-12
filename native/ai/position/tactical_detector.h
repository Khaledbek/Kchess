#pragma once

#include <string>
#include <vector>

#include "../dto/evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Tactical motif DTOs
// -----------------------------------------------------------------------------

enum class TacticalMotifKind {
  fork,
  royal_fork,
  double_attack,
  pin,
  skewer,
  discovered_attack,
  double_check,
  deflection,
  decoy,
  clearance,
  clearance_sacrifice,
  removal_of_defender,
  overloading,
  interference,
  zwischenzug,
  desperado,
  x_ray,
  battery,
  back_rank,
  greek_gift,
  smothered_mate,
};

struct TacticalMotif {
  TacticalMotifKind kind{TacticalMotifKind::double_attack};
  bool by_white{true};
  std::vector<int> attacker_squares;
  std::vector<int> target_squares;
  int blocker_square{-1};
  int trigger_from{-1};
  int trigger_to{-1};
  double confidence{1.0};
};

struct TacticalAnalysis {
  std::vector<TacticalMotif> motifs;
};

// -----------------------------------------------------------------------------
// Section: Deterministic detector
// -----------------------------------------------------------------------------

class TacticalDetector {
 public:
  [[nodiscard]] TacticalAnalysis analyze(const std::string& fen) const;
  [[nodiscard]] EvidenceItem evidence(const std::string& fen) const;
  [[nodiscard]] EvidenceItem evidence(const TacticalAnalysis& analysis) const;
};

}  // namespace kchess::ai
