#include "interaction/follow_up_suggestions.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace kchess::ai::interaction {
namespace {

void add_unique(std::vector<FollowUpAction>& out,
                std::size_t limit,
                std::string_view id,
                std::string_view action_id,
                std::string_view label_key,
                const ConversationState& state,
                bool attach_evidence = false,
                bool attach_line = false,
                bool attach_move = false) {
  if (out.size() >= limit) return;
  const auto duplicate = std::find_if(
      out.begin(), out.end(), [&](const FollowUpAction& existing) {
        return existing.action_id == action_id;
      });
  if (duplicate != out.end()) return;

  FollowUpAction item;
  item.id = std::string{id};
  item.action_id = std::string{action_id};
  item.label_key = std::string{label_key};
  if (attach_evidence) item.evidence_id = state.active_evidence_id;
  if (attach_line) item.line_id = state.active_line_id;
  if (attach_move) item.move_uci = state.active_move_uci;
  out.push_back(std::move(item));
}

bool has_any_topic(const ConversationState& state,
                   std::initializer_list<std::string_view> topics) {
  if (!state.active_topic.has_value()) return false;
  return std::find(topics.begin(), topics.end(), *state.active_topic) != topics.end();
}

void add_reference_followups(std::vector<FollowUpAction>& out,
                             std::size_t limit,
                             const ConversationState& state) {
  if (state.active_line_id.has_value()) {
    add_unique(out, limit, "followup.show_line", "show_line",
               "coachFollowupShowLine", state, false, true, false);
    add_unique(out, limit, "followup.explain_line", "explain_line",
               "coachFollowupExplainLine", state, true, true, false);
  }
  if (state.active_move_uci.has_value()) {
    add_unique(out, limit, "followup.show_move", "show_move",
               "coachFollowupShowMove", state, false, false, true);
    add_unique(out, limit, "followup.explain_move", "explain_move",
               "coachFollowupExplainMove", state, true, false, true);
  }
}

void add_topic_followups(std::vector<FollowUpAction>& out,
                         std::size_t limit,
                         const ConversationState& state) {
  if (has_any_topic(state, {"tactics", "material_gain", "material_loss",
                            "tactical_loss", "mate", "forced_mate", "trap",
                            "sacrifice"})) {
    add_unique(out, limit, "followup.show_arrows", "show_arrows",
               "coachFollowupShowArrows", state, true, true, true);
    add_unique(out, limit, "followup.show_candidates", "show_candidates",
               "coachFollowupShowAlternatives", state, true, false, false);
    add_unique(out, limit, "followup.compare_moves", "compare_moves",
               "coachFollowupCompareMoves", state, true, false, false);
  }

  if (has_any_topic(state, {"human_likelihood", "rating_prediction"})) {
    add_unique(out, limit, "followup.compare_moves", "compare_moves",
               "coachFollowupCompareWithBest", state, true, false, false);
    add_unique(out, limit, "followup.change_rating", "change_rating",
               "coachFollowupChangeRating", state, false, false, false);
  }

  if (has_any_topic(state, {"opening", "opening_frequency"})) {
    add_unique(out, limit, "followup.show_candidates", "show_candidates",
               "coachFollowupShowAlternatives", state, true, false, false);
    add_unique(out, limit, "followup.explain_move", "explain_move",
               "coachFollowupExplainIdea", state, true, false, true);
  }

  if (has_any_topic(state, {"strategy", "initiative", "attack", "defense",
                            "development", "positional_loss", "endgame"})) {
    add_unique(out, limit, "followup.explain_line", "explain_line",
               "coachFollowupExplainPlan", state, true, true, false);
    add_unique(out, limit, "followup.show_candidates", "show_candidates",
               "coachFollowupShowAlternatives", state, true, false, false);
  }
}

void add_plan_followups(std::vector<FollowUpAction>& out,
                        std::size_t limit,
                        const ConversationState& state,
                        const ResolvedInteractionPlan& plan) {
  if (plan.has_need("comparison")) {
    add_unique(out, limit, "followup.compare_moves", "compare_moves",
               "coachFollowupCompareMoves", state, true, false, false);
  }
  if (plan.has_need("principal_variations")) {
    add_unique(out, limit, "followup.show_line", "show_line",
               "coachFollowupShowLine", state, false, true, false);
  }
  if (plan.has_need("human_move_prediction")) {
    add_unique(out, limit, "followup.change_rating", "change_rating",
               "coachFollowupChangeRating", state, false, false, false);
  }
  if (plan.has_action(PrimitiveAction::start_human_bot) ||
      plan.has_action(PrimitiveAction::start_stockfish_game)) {
    add_unique(out, limit, "followup.enable_coaching", "enable_live_coaching",
               "coachFollowupEnableCoaching", state, false, false, false);
  }
}

}  // namespace

std::vector<FollowUpAction> FollowUpSuggestionsEngine::build(
    const FollowUpSuggestionContext& context,
    std::size_t limit) const {
  std::vector<FollowUpAction> out;
  if (limit == 0 || context.state == nullptr) return out;

  const ConversationState& state = *context.state;
  add_reference_followups(out, limit, state);
  add_topic_followups(out, limit, state);
  if (context.plan != nullptr) add_plan_followups(out, limit, state, *context.plan);

  // Always prefer actions that can reuse current evidence/session state. A
  // generic answer/explanation suggestion is only added when we still have room
  // and there is an active evidence reference to ground it.
  if (state.active_evidence_id.has_value()) {
    add_unique(out, limit, "followup.explain_more", "answer",
               "coachFollowupExplainMore", state, true, false, false);
  }

  return out;
}

}  // namespace kchess::ai::interaction
