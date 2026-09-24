#pragma once

#include "../coach_types.h"
#include "../dto/coach_request.h"
#include "../dto/query_plan.h"
#include "../dto/evidence.h"

#include <vector>
#include "teaching_plan.h"

namespace kchess::ai {

struct CoachSessionState;

class TeachingPlanner {
 public:
  [[nodiscard]] TeachingPlan plan(const CoachRequest& request,
                                  CoachIntent topic,
                                  const QueryPlan& query_plan,
                                  const std::vector<EvidenceItem>& evidence,
                                  const CoachSessionState* session = nullptr) const;
};

}  // namespace kchess::ai
