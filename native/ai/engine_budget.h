#pragma once

#include <vector>

#include "coach_types.h"
#include "dto/coach_request.h"
#include "dto/evidence.h"
#include "dto/query_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Engine budget decision
// -----------------------------------------------------------------------------

struct EngineBudgetDecision {
  EngineBudget requested{EngineBudget::none};
  EngineBudget effective{EngineBudget::none};
  bool satisfied_by_existing{false};

  [[nodiscard]] bool should_query() const {
    return effective != EngineBudget::none;
  }
};

class EngineBudgetSystem {
 public:
  [[nodiscard]] EngineBudgetDecision decide(
      const CoachRequest& request,
      const QueryPlan& plan,
      const std::vector<EvidenceItem>& available) const;
};

}  // namespace kchess::ai
