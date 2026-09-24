#include "interaction/semantic_selection.h"

#include <algorithm>

namespace kchess::ai::interaction {

std::string_view to_string(const InteractionRequestKind kind) noexcept {
  switch (kind) {
    case InteractionRequestKind::analysis:
      return "analysis";
    case InteractionRequestKind::app_action:
      return "app_action";
    case InteractionRequestKind::conversation_control:
      return "conversation_control";
    case InteractionRequestKind::question:
    default:
      return "question";
  }
}

std::string_view to_string(const InteractionAnswerIntent intent) noexcept {
  switch (intent) {
    case InteractionAnswerIntent::answer:
      return "answer";
    case InteractionAnswerIntent::explain:
      return "explain";
    case InteractionAnswerIntent::coach:
      return "coach";
    case InteractionAnswerIntent::none:
    default:
      return "none";
  }
}

bool PlannerSelection::contains(const std::string_view id) const {
  return std::any_of(tokens.begin(), tokens.end(),
                     [id](const PlannerSelectionToken& token) {
                       return token.id == id;
                     });
}

std::vector<std::string> PlannerSelection::ids_for(
    const SemanticGroup group) const {
  std::vector<std::string> result;
  for (const auto& token : tokens) {
    if (token.group == group) result.push_back(token.id);
  }
  return result;
}

}  // namespace kchess::ai::interaction
