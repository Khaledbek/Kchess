#pragma once

#include <string>
#include <vector>

#include "interaction/action_catalog.h"
#include "interaction/conversation_state.h"
#include "interaction/interaction_requirement_resolver.h"

namespace kchess::ai::interaction {

enum class ActionChainStepKind {
  action,
  evidence_gate,
  session_gate,
};

struct ActionChainStep {
  ActionChainStepKind kind{ActionChainStepKind::action};
  PrimitiveAction action{PrimitiveAction::none};
  std::string id;
  bool requires_previous_success{true};
};

struct ActionChain {
  std::vector<ActionChainStep> steps;
  bool valid{true};
  std::vector<std::string> diagnostics;

  [[nodiscard]] bool contains(PrimitiveAction action) const noexcept;
};

// Converts a resolved interaction plan into a deterministic, ordered execution
// chain. The engine does not execute providers, engine jobs, UI work, or model
// calls; it only defines the order and gates for later orchestration.
class ActionChainEngine {
 public:
  [[nodiscard]] ActionChain build(
      const ResolvedInteractionPlan& plan,
      const ConversationState& state = {}) const;
};

}  // namespace kchess::ai::interaction
