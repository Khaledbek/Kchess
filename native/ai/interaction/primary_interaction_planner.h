#pragma once

#include <string_view>

#include "interaction/conversation_state.h"
#include "interaction/semantic_selection.h"

namespace kchess::ai::interaction {

// Deterministic primary semantic classifier used before evidence retrieval.
// It emits only canonical dictionary IDs and never executes actions directly.
// The classifier intentionally stays bounded and deterministic. Extend the
// canonical semantic dictionary/rules rather than adding a second learned planner.
class PrimaryInteractionPlanner {
 public:
  [[nodiscard]] PlannerSelection classify(
      std::string_view user_text,
      const ConversationState& state = {}) const;
};

}  // namespace kchess::ai::interaction
