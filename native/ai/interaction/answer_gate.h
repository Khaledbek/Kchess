#pragma once

#include <string>

#include "interaction/action_chain_engine.h"
#include "interaction/interaction_requirement_resolver.h"

namespace kchess::ai::interaction {

struct AnswerGateDecision {
  bool answer_llm_required{false};
  std::string reason{"no_language_action"};
};

// Decides whether a resolved interaction needs a second language-model call.
// Board, navigation and game/session actions complete deterministically whenever
// they do not contain an explicit natural-language action.
class AnswerGate {
 public:
  [[nodiscard]] AnswerGateDecision decide(
      const ResolvedInteractionPlan& plan,
      const ActionChain& chain) const noexcept;
};

}  // namespace kchess::ai::interaction
