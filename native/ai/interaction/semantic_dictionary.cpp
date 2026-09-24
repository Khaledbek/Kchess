#include "interaction/semantic_dictionary.h"

#include <algorithm>

namespace kchess::ai::interaction {
namespace {

const std::vector<SemanticToken> kTokens = {
    // Tasks: what the user wants the system to accomplish.
    {"answer", SemanticGroup::task},
    {"explain", SemanticGroup::task},
    {"compare", SemanticGroup::task},
    {"predict", SemanticGroup::task},
    {"review", SemanticGroup::task},
    {"summarize", SemanticGroup::task},
    {"recommend", SemanticGroup::task},
    {"find_best", SemanticGroup::task},
    {"find_worst", SemanticGroup::task},
    {"find_losing_move", SemanticGroup::task},
    {"find_fastest_loss", SemanticGroup::task},
    {"find_fastest_win", SemanticGroup::task},
    {"find_mistake", SemanticGroup::task},
    {"find_blunder", SemanticGroup::task},
    {"find_tactic", SemanticGroup::task},
    {"find_plan", SemanticGroup::task},
    {"find_pattern", SemanticGroup::task},
    {"evaluate_move", SemanticGroup::task},
    {"evaluate_position", SemanticGroup::task},
    {"evaluate_game", SemanticGroup::task},
    {"start_game", SemanticGroup::task},
    {"continue_game", SemanticGroup::task},
    {"stop_game", SemanticGroup::task},
    {"train", SemanticGroup::task},

    // Scope: where the request applies.
    {"current_position", SemanticGroup::scope},
    {"selected_move", SemanticGroup::scope},
    {"last_move", SemanticGroup::scope},
    {"current_game", SemanticGroup::scope},
    {"game_history", SemanticGroup::scope},
    {"player_profile", SemanticGroup::scope},
    {"opening_subset", SemanticGroup::scope},
    {"conversation", SemanticGroup::scope},
    {"general_chess", SemanticGroup::scope},
    {"active_line", SemanticGroup::scope},
    {"active_finding", SemanticGroup::scope},

    // Subject / perspective.
    {"user", SemanticGroup::subject},
    {"opponent", SemanticGroup::subject},
    {"white", SemanticGroup::subject},
    {"black", SemanticGroup::subject},
    {"side_to_move", SemanticGroup::subject},
    {"both_players", SemanticGroup::subject},
    {"generic", SemanticGroup::subject},

    // Topics / subsubjects. These are intentionally compositional.
    {"material_loss", SemanticGroup::topic},
    {"material_gain", SemanticGroup::topic},
    {"king_safety", SemanticGroup::topic},
    {"mate", SemanticGroup::topic},
    {"forced_mate", SemanticGroup::topic},
    {"tactical_loss", SemanticGroup::topic},
    {"positional_loss", SemanticGroup::topic},
    {"tempo_loss", SemanticGroup::topic},
    {"development", SemanticGroup::topic},
    {"initiative", SemanticGroup::topic},
    {"attack", SemanticGroup::topic},
    {"defense", SemanticGroup::topic},
    {"draw", SemanticGroup::topic},
    {"repetition", SemanticGroup::topic},
    {"stalemate", SemanticGroup::topic},
    {"sacrifice", SemanticGroup::topic},
    {"trap", SemanticGroup::topic},
    {"blunder", SemanticGroup::topic},
    {"opening", SemanticGroup::topic},
    {"middlegame", SemanticGroup::topic},
    {"endgame", SemanticGroup::topic},
    {"calculation", SemanticGroup::topic},
    {"strategy", SemanticGroup::topic},
    {"tactics", SemanticGroup::topic},
    {"practical_chances", SemanticGroup::topic},
    {"human_likelihood", SemanticGroup::topic},
    {"rating_prediction", SemanticGroup::topic},
    {"time_pressure", SemanticGroup::topic},
    {"candidate_quality", SemanticGroup::topic},
    {"best_defense", SemanticGroup::topic},
    {"alternative_line", SemanticGroup::topic},
    {"opening_frequency", SemanticGroup::topic},
    {"player_tendency", SemanticGroup::topic},
    {"training_priority", SemanticGroup::topic},

    // Primitive interaction actions. Update 2 maps these to executable skills.
    {"show_line", SemanticGroup::action},
    {"show_move", SemanticGroup::action},
    {"show_arrows", SemanticGroup::action},
    {"show_candidates", SemanticGroup::action},
    {"explain_line", SemanticGroup::action},
    {"explain_move", SemanticGroup::action},
    {"compare_moves", SemanticGroup::action},
    {"play_move", SemanticGroup::action},
    {"start_human_bot", SemanticGroup::action},
    {"start_stockfish_game", SemanticGroup::action},
    {"resume_bot_game", SemanticGroup::action},
    {"resign_active_bot_game", SemanticGroup::action},
    {"enable_live_coaching", SemanticGroup::action},
    {"disable_live_coaching", SemanticGroup::action},
    {"change_rating", SemanticGroup::action},
    {"load_profile", SemanticGroup::action},
    {"load_opening", SemanticGroup::action},
    {"load_game_history", SemanticGroup::action},
    {"analyze_position", SemanticGroup::action},
    {"analyze_move", SemanticGroup::action},
    {"analyze_game", SemanticGroup::action},

    // Evidence needs.
    {"candidate_moves", SemanticGroup::need},
    {"move_evaluations", SemanticGroup::need},
    {"principal_variations", SemanticGroup::need},
    {"human_move_prediction", SemanticGroup::need},
    {"player_tendencies", SemanticGroup::need},
    {"historical_examples", SemanticGroup::need},
    {"comparison", SemanticGroup::need},
    {"move_explanation", SemanticGroup::need},
    {"tactical_motifs", SemanticGroup::need},
    {"strategic_plans", SemanticGroup::need},
    {"opening_context", SemanticGroup::need},
    {"rule_explanation", SemanticGroup::need},
    {"endgame_assessment", SemanticGroup::need},
    {"compensation", SemanticGroup::need},
    {"time_pressure_context", SemanticGroup::need},

    // Evidence sources.
    {"existing_analysis", SemanticGroup::source},
    {"engine", SemanticGroup::source},
    {"human_model", SemanticGroup::source},
    {"position_features", SemanticGroup::source},
    {"tactical_motifs_source", SemanticGroup::source},
    {"strategic_plans_source", SemanticGroup::source},
    {"opening_source", SemanticGroup::source},
    {"profile", SemanticGroup::source},
    {"game_history_source", SemanticGroup::source},
    {"conversation_state", SemanticGroup::source},
    {"rules_concepts", SemanticGroup::source},

    // Modifiers that shape the resolved action without inventing new actions.
    {"live_coaching", SemanticGroup::modifier},
    {"with_hints", SemanticGroup::modifier},
    {"without_hints", SemanticGroup::modifier},
    {"fast", SemanticGroup::modifier},
    {"deep", SemanticGroup::modifier},
    {"human_style", SemanticGroup::modifier},
    {"engine_style", SemanticGroup::modifier},
};

}  // namespace

const std::vector<SemanticToken>& semantic_dictionary() { return kTokens; }

std::optional<SemanticGroup> semantic_group_for(std::string_view id) {
  const auto it = std::find_if(kTokens.begin(), kTokens.end(),
                               [id](const SemanticToken& token) {
                                 return token.id == id;
                               });
  if (it == kTokens.end()) return std::nullopt;
  return it->group;
}

bool is_known_semantic_id(std::string_view id) {
  return semantic_group_for(id).has_value();
}

}  // namespace kchess::ai::interaction
