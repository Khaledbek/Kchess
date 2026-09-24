#include "interaction/answer_gate.h"

namespace kchess::ai::interaction {
namespace {

bool is_language_action(PrimitiveAction action) noexcept {
  return action == PrimitiveAction::answer_with_evidence ||
         action == PrimitiveAction::explain_line ||
         action == PrimitiveAction::explain_move ||
         action == PrimitiveAction::compare_moves;
}

}  // namespace

AnswerGateDecision AnswerGate::decide(const ResolvedInteractionPlan& plan,
                                      const ActionChain& chain) const noexcept {
  if (!plan.valid || !chain.valid) {
    return {false, "invalid_plan_or_chain"};
  }

  for (const auto& step : chain.steps) {
    if (step.kind == ActionChainStepKind::action &&
        is_language_action(step.action)) {
      return {true, "explicit_language_action"};
    }
  }

  // Analysis, board presentation and session mutations are native actions.
  // Evidence requirements alone must never promote them to an Answer-LLM call.
  if (plan.requires_language_response) {
    return {true, "resolved_plan_requires_language"};
  }

  return {false, "native_action_chain_complete"};
}

}  // namespace kchess::ai::interaction
