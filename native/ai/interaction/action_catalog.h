#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace kchess::ai::interaction {

enum class PrimitiveAction {
  none,
  answer_with_evidence,
  analyze_position,
  analyze_move,
  analyze_game,
  show_line,
  show_move,
  show_arrows,
  show_candidates,
  explain_line,
  explain_move,
  compare_moves,
  play_move,
  start_human_bot,
  start_stockfish_game,
  resume_bot_game,
  resign_active_bot_game,
  enable_live_coaching,
  disable_live_coaching,
  change_rating,
  load_profile,
  load_opening,
  load_game_history,
};

struct ActionDescriptor {
  PrimitiveAction action{PrimitiveAction::none};
  std::string_view semantic_id;
  bool requires_board{false};
  bool requires_language_response{false};
  bool mutates_session{false};
};

[[nodiscard]] const std::vector<ActionDescriptor>& action_catalog();
[[nodiscard]] std::optional<ActionDescriptor> action_for_id(std::string_view id);

}  // namespace kchess::ai::interaction
