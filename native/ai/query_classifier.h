#pragma once

#include "domain_router.h"
#include "dto/coach_request.h"
#include "dto/query_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coarse coach-query classification
// -----------------------------------------------------------------------------

struct CoachQueryClassification {
  QueryFamily family{QueryFamily::unknown};
  CoachIntent intent{CoachIntent::unknown};
  bool needs_profile{false};
  bool needs_position{false};
  bool needs_concepts{false};
  bool may_need_engine{false};
  ProfileQueryScope profile_scope;
  double confidence{0.0};
};

class CoachQueryClassifier {
 public:
  [[nodiscard]] CoachQueryClassification classify(
      const CoachRequest& request, const DomainRoute& route) const;
};

}  // namespace kchess::ai
