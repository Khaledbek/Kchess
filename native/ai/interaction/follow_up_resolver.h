#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "interaction/conversation_state.h"

namespace kchess::ai::interaction {

enum class FollowUpResolutionKind {
  none,
  deterministic,
  ambiguous,
};

struct FollowUpResolution {
  FollowUpResolutionKind kind{FollowUpResolutionKind::none};
  std::optional<FollowUpAction> followup;
  std::string reason;
};

class FollowUpResolver {
 public:
  [[nodiscard]] FollowUpResolution resolve(
      std::string_view user_text,
      const ConversationState& state) const;
  [[nodiscard]] FollowUpResolution resolve_action_id(
      std::string_view action_id,
      const ConversationState& state) const;
};

}  // namespace kchess::ai::interaction
