#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "interaction/action_chain_engine.h"
#include "interaction/conversation_state.h"
#include "interaction/interaction_requirement_resolver.h"

namespace kchess::ai::interaction {

struct FollowUpSuggestionContext {
  const ConversationState* state{nullptr};
  const ResolvedInteractionPlan* plan{nullptr};
  const ActionChain* chain{nullptr};
};

// Produces structured follow-up actions for the current interaction. Suggestions
// are deterministic and reference already-known session/evidence identifiers so
// a UI click can be executed without a new planner LLM request.
class FollowUpSuggestionsEngine {
 public:
  [[nodiscard]] std::vector<FollowUpAction> build(
      const FollowUpSuggestionContext& context,
      std::size_t limit = 4) const;
};

}  // namespace kchess::ai::interaction
