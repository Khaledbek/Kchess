#include "interaction/action_chain_engine.h"

#include <algorithm>

namespace kchess::ai::interaction {
namespace {

void add_step(ActionChain& chain, ActionChainStep step) {
  const auto duplicate = std::find_if(
      chain.steps.begin(), chain.steps.end(), [&](const ActionChainStep& existing) {
        return existing.kind == step.kind && existing.action == step.action &&
               existing.id == step.id;
      });
  if (duplicate == chain.steps.end()) chain.steps.push_back(std::move(step));
}

void add_action(ActionChain& chain, PrimitiveAction action) {
  if (action == PrimitiveAction::none) return;
  add_step(chain, {ActionChainStepKind::action, action, {}, true});
}

bool is_game_start(PrimitiveAction action) {
  return action == PrimitiveAction::start_human_bot ||
         action == PrimitiveAction::start_stockfish_game;
}

bool is_game_session_action(PrimitiveAction action) {
  return action == PrimitiveAction::resume_bot_game ||
         action == PrimitiveAction::resign_active_bot_game;
}

bool is_board_presentation(PrimitiveAction action) {
  return action == PrimitiveAction::show_line || action == PrimitiveAction::show_move ||
         action == PrimitiveAction::show_arrows ||
         action == PrimitiveAction::show_candidates;
}

bool is_language_action(PrimitiveAction action) {
  return action == PrimitiveAction::answer_with_evidence ||
         action == PrimitiveAction::explain_line ||
         action == PrimitiveAction::explain_move ||
         action == PrimitiveAction::compare_moves;
}

void add_evidence_gate_if_needed(const ResolvedInteractionPlan& plan,
                                 ActionChain& chain) {
  if (plan.sources.empty() && plan.needs.empty()) return;
  add_step(chain, {ActionChainStepKind::evidence_gate,
                   PrimitiveAction::none,
                   "evidence_ready",
                   true});
}

}  // namespace

bool ActionChain::contains(PrimitiveAction action) const noexcept {
  return std::any_of(steps.begin(), steps.end(), [action](const ActionChainStep& step) {
    return step.kind == ActionChainStepKind::action && step.action == action;
  });
}

ActionChain ActionChainEngine::build(const ResolvedInteractionPlan& plan,
                                     const ConversationState& state) const {
  ActionChain chain;
  if (!plan.valid) {
    chain.valid = false;
    chain.diagnostics = plan.diagnostics;
    chain.diagnostics.emplace_back("resolved_plan_invalid");
    return chain;
  }

  // Session mutations that configure a game must happen before the game starts.
  if (plan.has_action(PrimitiveAction::change_rating)) {
    add_action(chain, PrimitiveAction::change_rating);
  }

  for (const auto action : plan.actions) {
    if (is_game_start(action) || is_game_session_action(action)) {
      add_action(chain, action);
    }
  }

  if (plan.has_action(PrimitiveAction::enable_live_coaching)) {
    if (!chain.contains(PrimitiveAction::start_human_bot) &&
        !chain.contains(PrimitiveAction::start_stockfish_game) &&
        !state.game_id.has_value()) {
      chain.valid = false;
      chain.diagnostics.emplace_back("live_coaching_requires_active_or_starting_game");
    } else {
      add_step(chain, {ActionChainStepKind::session_gate,
                       PrimitiveAction::none,
                       "game_session_ready",
                       true});
      add_action(chain, PrimitiveAction::enable_live_coaching);
    }
  }

  if (plan.has_action(PrimitiveAction::disable_live_coaching)) {
    add_action(chain, PrimitiveAction::disable_live_coaching);
  }

  // Analysis/load operations produce or expose evidence and therefore precede
  // any board presentation or natural-language rendering.
  for (const auto action : plan.actions) {
    if (action == PrimitiveAction::analyze_position ||
        action == PrimitiveAction::analyze_move ||
        action == PrimitiveAction::analyze_game || action == PrimitiveAction::load_profile ||
        action == PrimitiveAction::load_opening ||
        action == PrimitiveAction::load_game_history) {
      add_action(chain, action);
    }
  }

  add_evidence_gate_if_needed(plan, chain);

  for (const auto action : plan.actions) {
    if (is_board_presentation(action)) add_action(chain, action);
  }

  if (plan.has_action(PrimitiveAction::play_move)) {
    add_action(chain, PrimitiveAction::play_move);
  }

  // Language rendering is deliberately last so it can consume already resolved
  // evidence and UI/game actions never need to wait on prose generation.
  for (const auto action : plan.actions) {
    if (is_language_action(action)) add_action(chain, action);
  }

  if (chain.steps.empty()) {
    chain.valid = false;
    chain.diagnostics.emplace_back("empty_action_chain");
  }

  return chain;
}

}  // namespace kchess::ai::interaction
