#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "interaction/semantic_dictionary.h"

namespace kchess::ai::interaction {

// Coarse current-turn contract used before evidence planning. This is not a
// chess-domain intent taxonomy: it separates language/analysis requests from
// direct product/session actions so app actions cannot silently fall through to
// positional explanation just because a board is present.
enum class InteractionRequestKind {
  question,
  analysis,
  app_action,
  conversation_control,
};

// Language responsibility is orthogonal to product/session actions. A turn may
// execute a native action and still require prose (for example "start a game
// and coach me while we play").
enum class InteractionAnswerIntent {
  none,
  answer,
  explain,
  coach,
};

[[nodiscard]] std::string_view to_string(InteractionRequestKind kind) noexcept;
[[nodiscard]] std::string_view to_string(InteractionAnswerIntent intent) noexcept;

struct PlannerSelectionToken {
  std::string id;
  SemanticGroup group;
};

struct PlannerSelection {
  InteractionRequestKind request_kind{InteractionRequestKind::question};
  std::vector<PlannerSelectionToken> tokens;

  [[nodiscard]] bool empty() const noexcept { return tokens.empty(); }
  [[nodiscard]] bool contains(std::string_view id) const;
  [[nodiscard]] std::vector<std::string> ids_for(SemanticGroup group) const;
};

}  // namespace kchess::ai::interaction
