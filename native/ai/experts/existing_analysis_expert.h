#pragma once

#include <optional>
#include <vector>

#include "candidate_move_system.h"
#include "dto/evidence.h"
#include "dto/evidence_plan.h"
#include "dto/query_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Existing-analysis coverage contract
// -----------------------------------------------------------------------------

struct ExistingAnalysisCoverage {
  std::vector<EvidenceNeed> satisfied_needs;
  bool has_persisted_analysis{false};
  bool has_candidate_set{false};

  [[nodiscard]] bool satisfies(EvidenceNeed need) const;
};

// Interprets already-available KChess analysis as planner-visible evidence.
// It never starts engine work. Its only job is to say which requested facts are
// actually proven by the persisted/cache data that was retrieved for this FEN.
class ExistingAnalysisExpert {
 public:
  [[nodiscard]] ExistingAnalysisCoverage inspect(
      const QueryPlan& plan,
      const std::vector<EvidenceItem>& items,
      const std::optional<CandidateMoveSet>& candidates) const;
};

}  // namespace kchess::ai
