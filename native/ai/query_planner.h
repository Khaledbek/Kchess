#pragma once

#include "domain_router.h"
#include "dto/coach_request.h"
#include "dto/query_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Evidence and budget planning
// -----------------------------------------------------------------------------

class QueryPlanner {
 public:
  [[nodiscard]] QueryPlan plan(
      const CoachRequest& request, const DomainRoute& route) const;
};

}  // namespace kchess::ai
