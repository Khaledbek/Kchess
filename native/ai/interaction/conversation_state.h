#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kchess::ai::interaction {

struct FollowUpAction {
  std::string id;
  std::string action_id;
  std::string label_key;
  std::optional<std::string> evidence_id;
  std::optional<std::string> line_id;
  std::optional<std::string> move_uci;
};

struct ConversationState {
  std::optional<std::string> position_key;
  std::optional<std::string> game_id;
  std::optional<std::string> active_topic;
  std::optional<std::string> active_subject;
  std::optional<std::string> active_evidence_id;
  std::optional<std::string> active_line_id;
  std::optional<std::string> active_move_uci;
  std::optional<int> rating_target;
  std::optional<std::string> interaction_mode;
  std::optional<std::string> last_user_action;
  std::optional<std::string> last_coach_action;
  std::vector<FollowUpAction> available_followups;

  [[nodiscard]] bool has_active_reference() const;
  [[nodiscard]] bool has_followup(std::string_view action_id) const;
};

}  // namespace kchess::ai::interaction
