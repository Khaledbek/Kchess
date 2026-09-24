#include "interaction/interaction_requirement_resolver.h"

#include <algorithm>

namespace kchess::ai::interaction {
namespace {

template <typename T>
void add_unique(std::vector<T>& values, T value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(std::move(value));
  }
}

void add_source(ResolvedInteractionPlan& plan, std::string_view id) {
  add_unique(plan.sources, std::string{id});
}

void add_need(ResolvedInteractionPlan& plan, std::string_view id) {
  add_unique(plan.needs, std::string{id});
}

void add_action(ResolvedInteractionPlan& plan, PrimitiveAction action) {
  if (action == PrimitiveAction::none) return;
  add_unique(plan.actions, action);
  const auto& catalog = action_catalog();
  const auto it = std::find_if(catalog.begin(), catalog.end(), [action](const auto& item) {
    return item.action == action;
  });
  if (it != catalog.end()) {
    plan.requires_board = plan.requires_board || it->requires_board;
    plan.requires_language_response =
        plan.requires_language_response || it->requires_language_response;
    plan.mutates_session = plan.mutates_session || it->mutates_session;
  }
}

void add_engine_analysis(ResolvedInteractionPlan& plan, bool with_pv = true) {
  add_source(plan, "existing_analysis");
  add_source(plan, "engine");
  add_need(plan, "candidate_moves");
  add_need(plan, "move_evaluations");
  if (with_pv) add_need(plan, "principal_variations");
}

void apply_task(std::string_view id, ResolvedInteractionPlan& plan) {
  if (id == "answer") {
    add_action(plan, PrimitiveAction::answer_with_evidence);
  } else if (id == "explain") {
    add_action(plan, PrimitiveAction::answer_with_evidence);
    add_need(plan, "move_explanation");
  } else if (id == "compare") {
    add_action(plan, PrimitiveAction::compare_moves);
    add_engine_analysis(plan);
    add_need(plan, "comparison");
  } else if (id == "predict") {
    add_source(plan, "human_model");
    add_need(plan, "human_move_prediction");
    add_action(plan, PrimitiveAction::answer_with_evidence);
  } else if (id == "review" || id == "evaluate_game") {
    add_action(plan, PrimitiveAction::analyze_game);
    add_source(plan, "game_history_source");
  } else if (id == "summarize") {
    add_action(plan, PrimitiveAction::answer_with_evidence);
  } else if (id == "recommend" || id == "find_plan") {
    add_source(plan, "strategic_plans_source");
    add_need(plan, "strategic_plans");
    add_action(plan, PrimitiveAction::answer_with_evidence);
  } else if (id == "find_best") {
    add_action(plan, PrimitiveAction::analyze_position);
    add_engine_analysis(plan);
  } else if (id == "find_worst" || id == "find_losing_move" ||
             id == "find_fastest_loss") {
    add_action(plan, PrimitiveAction::analyze_position);
    add_engine_analysis(plan);
    add_need(plan, "comparison");
  } else if (id == "find_fastest_win") {
    add_action(plan, PrimitiveAction::analyze_position);
    add_engine_analysis(plan);
  } else if (id == "find_mistake" || id == "find_blunder" ||
             id == "evaluate_move") {
    add_action(plan, PrimitiveAction::analyze_move);
    add_engine_analysis(plan);
    add_need(plan, "move_explanation");
  } else if (id == "find_tactic") {
    add_action(plan, PrimitiveAction::analyze_position);
    add_source(plan, "tactical_motifs_source");
    add_need(plan, "tactical_motifs");
    add_engine_analysis(plan);
  } else if (id == "find_pattern") {
    add_source(plan, "profile");
    add_source(plan, "game_history_source");
    add_need(plan, "player_tendencies");
    add_need(plan, "historical_examples");
    add_action(plan, PrimitiveAction::answer_with_evidence);
  } else if (id == "evaluate_position") {
    add_action(plan, PrimitiveAction::analyze_position);
    add_engine_analysis(plan);
  } else if (id == "start_game") {
    // Default opponent remains Stockfish unless the semantic selection or
    // explicit human-style/rating parameter requests the human model.
    add_action(plan, PrimitiveAction::start_stockfish_game);
  } else if (id == "continue_game") {
    add_action(plan, PrimitiveAction::resume_bot_game);
  } else if (id == "stop_game") {
    add_action(plan, PrimitiveAction::resign_active_bot_game);
  } else if (id == "train") {
    add_action(plan, PrimitiveAction::enable_live_coaching);
  }
}

void apply_topic(std::string_view id, ResolvedInteractionPlan& plan) {
  if (id == "material_loss" || id == "material_gain" || id == "tactical_loss" ||
      id == "mate" || id == "forced_mate" || id == "best_defense" ||
      id == "candidate_quality") {
    add_engine_analysis(plan);
  }
  if (id == "tactics" || id == "trap" || id == "sacrifice") {
    add_source(plan, "tactical_motifs_source");
    add_need(plan, "tactical_motifs");
  }
  if (id == "strategy" || id == "initiative" || id == "attack" ||
      id == "defense" || id == "development" || id == "positional_loss") {
    add_source(plan, "strategic_plans_source");
    add_need(plan, "strategic_plans");
  }
  if (id == "opening" || id == "opening_frequency") {
    add_source(plan, "opening_source");
    add_need(plan, "opening_context");
  }
  if (id == "endgame") {
    add_need(plan, "endgame_assessment");
    add_engine_analysis(plan);
  }
  if (id == "human_likelihood" || id == "rating_prediction") {
    add_source(plan, "human_model");
    add_need(plan, "human_move_prediction");
  }
  if (id == "player_tendency" || id == "training_priority") {
    add_source(plan, "profile");
    add_source(plan, "game_history_source");
    add_need(plan, "player_tendencies");
  }
  if (id == "time_pressure") add_need(plan, "time_pressure_context");
}

void apply_scope(std::string_view id, ResolvedInteractionPlan& plan) {
  if (id == "player_profile") add_source(plan, "profile");
  if (id == "game_history" || id == "current_game") add_source(plan, "game_history_source");
  if (id == "opening_subset") add_source(plan, "opening_source");
  if (id == "conversation" || id == "active_line" || id == "active_finding") {
    add_source(plan, "conversation_state");
  }
}

void apply_explicit_action(std::string_view id, ResolvedInteractionPlan& plan) {
  if (const auto action = action_for_id(id)) add_action(plan, action->action);
}

void complete_dependencies(ResolvedInteractionPlan& plan) {
  if (plan.has_need("principal_variations") || plan.has_need("candidate_moves") ||
      plan.has_need("move_evaluations") || plan.has_need("comparison") ||
      plan.has_need("endgame_assessment") || plan.has_need("compensation")) {
    add_source(plan, "existing_analysis");
    add_source(plan, "engine");
  }
  if (plan.has_need("human_move_prediction")) add_source(plan, "human_model");
  if (plan.has_need("player_tendencies") || plan.has_need("historical_examples")) {
    add_source(plan, "profile");
    add_source(plan, "game_history_source");
  }
  if (plan.has_need("opening_context")) add_source(plan, "opening_source");
  if (plan.has_need("rule_explanation")) add_source(plan, "rules_concepts");
  if (plan.has_need("tactical_motifs")) add_source(plan, "tactical_motifs_source");
  if (plan.has_need("strategic_plans")) add_source(plan, "strategic_plans_source");
}

}  // namespace

bool ResolvedInteractionPlan::has_action(PrimitiveAction action) const noexcept {
  return std::find(actions.begin(), actions.end(), action) != actions.end();
}

bool ResolvedInteractionPlan::has_source(std::string_view source) const noexcept {
  return std::find(sources.begin(), sources.end(), source) != sources.end();
}

bool ResolvedInteractionPlan::has_need(std::string_view need) const noexcept {
  return std::find(needs.begin(), needs.end(), need) != needs.end();
}

ResolvedInteractionPlan InteractionRequirementResolver::resolve(
    const PlannerSelection& selection,
    const ParsedInteractionParameters& parameters,
    const ConversationState& state) const {
  ResolvedInteractionPlan plan;
  plan.request_kind = selection.request_kind;
  plan.elo = parameters.elo;
  plan.color = parameters.color;
  plan.move_reference = parameters.move_reference;
  plan.game_mode = parameters.game_mode;
  plan.time_control = parameters.time_control;

  for (const auto& token : selection.tokens) {
    add_unique(plan.semantic_ids, token.id);
    switch (token.group) {
      case SemanticGroup::task:
        apply_task(token.id, plan);
        break;
      case SemanticGroup::scope:
        apply_scope(token.id, plan);
        break;
      case SemanticGroup::topic:
        apply_topic(token.id, plan);
        break;
      case SemanticGroup::action:
        apply_explicit_action(token.id, plan);
        break;
      case SemanticGroup::need:
        add_need(plan, token.id);
        break;
      case SemanticGroup::source:
        add_source(plan, token.id);
        break;
      case SemanticGroup::modifier:
        if (token.id == "live_coaching" || token.id == "with_hints") {
          add_action(plan, PrimitiveAction::enable_live_coaching);
        } else if (token.id == "without_hints") {
          add_action(plan, PrimitiveAction::disable_live_coaching);
        } else if (token.id == "human_style") {
          add_source(plan, "human_model");
        }
        break;
      case SemanticGroup::subject:
        break;
    }
  }

  // Explicit rating + game intent strongly implies a human-strength opponent.
  if (parameters.elo.has_value() &&
      (selection.contains("start_game") || selection.contains("human_likelihood") ||
       selection.contains("rating_prediction") || selection.contains("predict"))) {
    add_source(plan, "human_model");
    if (selection.contains("start_game")) {
      plan.actions.erase(
          std::remove(plan.actions.begin(), plan.actions.end(),
                      PrimitiveAction::start_stockfish_game),
          plan.actions.end());
      add_action(plan, PrimitiveAction::start_human_bot);
      add_action(plan, PrimitiveAction::change_rating);
    }
  }

  // Active references are only requested when the semantic plan refers to them.
  if ((selection.contains("active_line") || selection.contains("active_finding")) &&
      !state.has_active_reference()) {
    plan.valid = false;
    plan.diagnostics.emplace_back("missing_active_conversation_reference");
  }

  complete_dependencies(plan);

  if (selection.contains("live_coaching")) {
    plan.answer_intent = InteractionAnswerIntent::coach;
  } else if (selection.contains("explain")) {
    plan.answer_intent = InteractionAnswerIntent::explain;
  } else if (plan.requires_language_response) {
    plan.answer_intent = InteractionAnswerIntent::answer;
  }

  // Evidence-only requests need a language response unless they are pure UI/game actions.
  if (!plan.needs.empty() && plan.actions.empty()) {
    add_action(plan, PrimitiveAction::answer_with_evidence);
    if (plan.answer_intent == InteractionAnswerIntent::none) {
      plan.answer_intent = InteractionAnswerIntent::answer;
    }
  }

  if (plan.actions.empty() && plan.needs.empty() && plan.sources.empty()) {
    plan.valid = false;
    plan.diagnostics.emplace_back("empty_resolved_interaction_plan");
  }

  return plan;
}

}  // namespace kchess::ai::interaction
