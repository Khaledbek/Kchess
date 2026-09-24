#pragma once

#include <string>
#include <vector>

#include "interaction/action_chain_engine.h"
#include "interaction/interaction_requirement_resolver.h"
#include "dto/coach_response.h"

namespace kchess::ai::interaction {

// Read-only verification of native product/session action transport. This does
// not execute UI work. It checks that every client-bound action selected by the
// deterministic interaction plan has a concrete machine action in the final
// CoachResponse. Provider prose can never satisfy this contract.
struct ActionFulfillmentReport {
  bool required{false};
  bool passed{true};
  std::vector<std::string> requested_actions;
  std::vector<std::string> transported_actions;
  std::vector<std::string> issues;
};

class ActionFulfillmentValidator {
 public:
  [[nodiscard]] ActionFulfillmentReport validate(
      const ResolvedInteractionPlan& plan,
      const ActionChain& chain,
      const CoachResponse& response) const;
};

}  // namespace kchess::ai::interaction
