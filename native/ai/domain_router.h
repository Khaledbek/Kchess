#pragma once

#include "coach_types.h"
#include "dto/coach_request.h"

namespace kchess::ai {
struct CoachSessionState;
class TinyIntentModel;
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
  CoachIntent context_intent{CoachIntent::unknown};
};

// -----------------------------------------------------------------------------
// Section: Chess-domain router
// -----------------------------------------------------------------------------

class ChessDomainRouter {
 public:
  [[nodiscard]] DomainRoute route(
      const CoachRequest& request,
      const CoachSessionState* session = nullptr,
      const TinyIntentModel* model = nullptr) const;
};

}  // namespace kchess::ai
