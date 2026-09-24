#pragma once

#include "coach_types.h"
#include "dto/coach_request.h"
#include "dto/query_plan.h"

namespace kchess::ai {
struct CoachSessionState;
}

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Routing result
// -----------------------------------------------------------------------------

struct DomainRoute {
  CoachIntent intent{CoachIntent::unknown};
  double confidence{0.0};
  bool chess_domain{false};
  bool follow_up{false};
  // True when the current user text itself resolves a concrete chess intent.
  // Session/teaching context may enrich such a turn but must never replace it.
  bool explicit_current_intent{false};
  CoachIntent context_intent{CoachIntent::unknown};
  QueryFamily inherited_query_family{QueryFamily::unknown};
  ProfileQueryScope inherited_profile_scope;
  bool inherited_needs_profile{false};
};

// -----------------------------------------------------------------------------
// Section: Chess-domain router
// -----------------------------------------------------------------------------

class ChessDomainRouter {
 public:
  [[nodiscard]] DomainRoute route(
      const CoachRequest& request,
      const CoachSessionState* session = nullptr) const;
};

}  // namespace kchess::ai
