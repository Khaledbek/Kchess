#pragma once

#include <optional>
#include <string>
#include <vector>

#include "interaction/action_catalog.h"
#include "interaction/conversation_state.h"
#include "interaction/parameter_parser.h"
#include "interaction/semantic_selection.h"

namespace kchess::ai::interaction {

struct ResolvedInteractionPlan {
  InteractionRequestKind request_kind{InteractionRequestKind::question};
  InteractionAnswerIntent answer_intent{InteractionAnswerIntent::none};
  std::vector<PrimitiveAction> actions;
  std::vector<std::string> sources;
  std::vector<std::string> needs;
  std::vector<std::string> semantic_ids;

  std::optional<int> elo;
  std::optional<RequestedColor> color;
  std::optional<MoveReference> move_reference;
  std::optional<RequestedGameMode> game_mode;
  std::optional<RequestedTimeControl> time_control;

  bool requires_board{false};
  bool requires_language_response{false};
  bool mutates_session{false};
  bool valid{true};
  std::vector<std::string> diagnostics;

  [[nodiscard]] bool has_action(PrimitiveAction action) const noexcept;
  [[nodiscard]] bool has_source(std::string_view source) const noexcept;
  [[nodiscard]] bool has_need(std::string_view need) const noexcept;
};

// Converts untrusted semantic selections plus deterministic parameters into a
// technically valid KChess interaction plan. The resolver may add mandatory
// dependencies, but it never invents arbitrary provider/tool names.
class InteractionRequirementResolver {
 public:
  [[nodiscard]] ResolvedInteractionPlan resolve(
      const PlannerSelection& selection,
      const ParsedInteractionParameters& parameters,
      const ConversationState& state = {}) const;
};

}  // namespace kchess::ai::interaction
