#include "interaction/conversation_state.h"

#include <algorithm>
#include <string_view>

namespace kchess::ai::interaction {

bool ConversationState::has_active_reference() const {
  return active_evidence_id.has_value() || active_line_id.has_value() ||
         active_move_uci.has_value();
}

bool ConversationState::has_followup(std::string_view action_id) const {
  return std::any_of(available_followups.begin(), available_followups.end(),
                     [action_id](const FollowUpAction& followup) {
                       return followup.action_id == action_id;
                     });
}

}  // namespace kchess::ai::interaction
