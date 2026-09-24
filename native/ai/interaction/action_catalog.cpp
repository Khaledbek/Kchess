#include "interaction/action_catalog.h"

#include <algorithm>

namespace kchess::ai::interaction {
namespace {

const std::vector<ActionDescriptor> kActions = {
    {PrimitiveAction::answer_with_evidence, "answer", false, true, false},
    {PrimitiveAction::analyze_position, "analyze_position", true, false, false},
    {PrimitiveAction::analyze_move, "analyze_move", true, false, false},
    {PrimitiveAction::analyze_game, "analyze_game", false, false, false},
    {PrimitiveAction::show_line, "show_line", true, false, false},
    {PrimitiveAction::show_move, "show_move", true, false, false},
    {PrimitiveAction::show_arrows, "show_arrows", true, false, false},
    {PrimitiveAction::show_candidates, "show_candidates", true, false, false},
    {PrimitiveAction::explain_line, "explain_line", false, true, false},
    {PrimitiveAction::explain_move, "explain_move", false, true, false},
    {PrimitiveAction::compare_moves, "compare_moves", false, true, false},
    {PrimitiveAction::play_move, "play_move", true, false, true},
    {PrimitiveAction::start_human_bot, "start_human_bot", false, false, true},
    {PrimitiveAction::start_stockfish_game, "start_stockfish_game", false, false, true},
    {PrimitiveAction::resume_bot_game, "resume_bot_game", false, false, true},
    {PrimitiveAction::resign_active_bot_game, "resign_active_bot_game", false, false, true},
    {PrimitiveAction::enable_live_coaching, "enable_live_coaching", false, false, true},
    {PrimitiveAction::disable_live_coaching, "disable_live_coaching", false, false, true},
    {PrimitiveAction::change_rating, "change_rating", false, false, true},
    {PrimitiveAction::load_profile, "load_profile", false, false, false},
    {PrimitiveAction::load_opening, "load_opening", false, false, false},
    {PrimitiveAction::load_game_history, "load_game_history", false, false, false},
};

}  // namespace

const std::vector<ActionDescriptor>& action_catalog() { return kActions; }

std::optional<ActionDescriptor> action_for_id(std::string_view id) {
  const auto it = std::find_if(kActions.begin(), kActions.end(),
                               [id](const ActionDescriptor& descriptor) {
                                 return descriptor.semantic_id == id;
                               });
  if (it == kActions.end()) return std::nullopt;
  return *it;
}

}  // namespace kchess::ai::interaction
