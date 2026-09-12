#include "query_planner.h"

#include <algorithm>

#include "models/small_models.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Intent policy
// -----------------------------------------------------------------------------

bool position_bound(CoachIntent intent) {
  switch (intent) {
    case CoachIntent::position:
    case CoachIntent::move_explanation:
    case CoachIntent::plan:
    case CoachIntent::tactic:
    case CoachIntent::game_review:
      return true;
    default:
      return false;
  }
}

bool profile_relevant(CoachIntent intent) {
  return intent == CoachIntent::player_development ||
         intent == CoachIntent::training || intent == CoachIntent::game_review;
}

bool concepts_relevant(CoachIntent intent) {
  switch (intent) {
    case CoachIntent::plan:
    case CoachIntent::tactic:
    case CoachIntent::opening:
    case CoachIntent::endgame:
    case CoachIntent::chess_concept:
    case CoachIntent::player_development:
    case CoachIntent::training:
      return true;
    default:
      return false;
  }
}

ResponseDepth choose_depth(const CoachRequest& request, CoachIntent intent) {
  if (request.mode == CoachMode::hint || request.mode == CoachMode::quiz) {
    return ResponseDepth::concise;
  }
  if (request.mode == CoachMode::teach || request.mode == CoachMode::review ||
      intent == CoachIntent::game_review) {
    return ResponseDepth::detailed;
  }
  return request.depth;
}

EngineBudget choose_engine_budget(const CoachRequest& request,
                                  CoachIntent intent) {
  if (!request.position_fen && !request.game_pgn) return EngineBudget::none;
  if (request.hint_move_uci || request.mode == CoachMode::hint || request.mode == CoachMode::quiz)
    return EngineBudget::probe;
  if (intent == CoachIntent::move_explanation || intent == CoachIntent::tactic ||
      intent == CoachIntent::game_review || request.mode == CoachMode::compare) {
    return EngineBudget::probe;
  }
  return EngineBudget::none;
}

void add_need(QueryPlan& plan, EvidenceKind kind) {
  if (std::find(plan.evidence.begin(), plan.evidence.end(), kind) ==
      plan.evidence.end()) {
    plan.evidence.push_back(kind);
  }
}

bool model_extra_allowed(EvidenceKind kind, bool has_position) {
  switch (kind) {
    case EvidenceKind::chess_concepts:
    case EvidenceKind::theory:
    case EvidenceKind::opening:
    case EvidenceKind::user_profile:
      return true;
    case EvidenceKind::position_features:
    case EvidenceKind::position_weaknesses:
    case EvidenceKind::weakness_exploitation:
    case EvidenceKind::tactical_motifs:
    case EvidenceKind::strategic_plans:
      return has_position;
    default:
      return false;
  }
}

void apply_context_model(QueryPlan& plan, const CoachRequest& request,
                         const DomainRoute& route,
                         const TinyContextPlannerModel* model) {
  if (model == nullptr || !model->available() || !route.chess_domain) return;
  const bool has_position = request.position_fen.has_value() || request.game_pgn.has_value();
  const ContextPlannerInput input{
      .user_text = request.user_text,
      .intent = route.intent,
      .mode = request.mode,
      .requested_depth = request.depth,
      .has_position = has_position,
      .has_game = request.game_pgn.has_value(),
      .has_session = request.session_id.has_value(),
  };
  const auto prediction = model->predict(input);
  if (!prediction.has_value() || prediction->confidence < 0.85) return;

  if (prediction->concept_limit > 0) {
    plan.concept_limit = std::clamp<std::size_t>(prediction->concept_limit, 2, 5);
  }
  for (const auto kind : prediction->extra_evidence) {
    if (model_extra_allowed(kind, has_position)) add_need(plan, kind);
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public planning
// -----------------------------------------------------------------------------

QueryPlan QueryPlanner::plan(const CoachRequest& request,
                             const DomainRoute& route,
                             const TinyContextPlannerModel* model) const {
  QueryPlan plan;
  plan.intent = route.intent;
  plan.context_intent = route.context_intent;
  plan.routing_confidence = route.confidence;
  plan.response_depth = choose_depth(request, route.intent);
  plan.has_conversation_context =
      route.context_intent != CoachIntent::unknown || route.follow_up;

  if (!route.chess_domain || route.intent == CoachIntent::off_topic ||
      route.intent == CoachIntent::unknown) {
    return plan;
  }

  const CoachIntent effective_intent =
      route.intent == CoachIntent::follow_up &&
              route.context_intent != CoachIntent::unknown
          ? route.context_intent
          : route.intent;
  const bool has_position = request.position_fen.has_value() ||
                            request.game_pgn.has_value();

  plan.needs_position = has_position && (position_bound(effective_intent) ||
      request.mode == CoachMode::compare || request.mode == CoachMode::hint ||
      request.mode == CoachMode::quiz || request.hint_move_uci.has_value());
  plan.needs_profile = profile_relevant(effective_intent);
  plan.needs_theory = effective_intent == CoachIntent::opening;
  plan.needs_opening = effective_intent == CoachIntent::opening;
  plan.engine_budget = choose_engine_budget(request, effective_intent);

  add_need(plan, EvidenceKind::cache);
  if (plan.has_conversation_context) add_need(plan, EvidenceKind::conversation);
  if (concepts_relevant(effective_intent)) {
    add_need(plan, EvidenceKind::chess_concepts);
  }
  if (plan.needs_position) {
    add_need(plan, EvidenceKind::existing_analysis);
    add_need(plan, EvidenceKind::position_features);
    if (effective_intent == CoachIntent::position ||
        effective_intent == CoachIntent::plan ||
        effective_intent == CoachIntent::game_review) {
      add_need(plan, EvidenceKind::position_weaknesses);
      add_need(plan, EvidenceKind::weakness_exploitation);
      add_need(plan, EvidenceKind::strategic_plans);
    }
    if (effective_intent == CoachIntent::position ||
        effective_intent == CoachIntent::move_explanation ||
        effective_intent == CoachIntent::tactic ||
        effective_intent == CoachIntent::game_review) {
      add_need(plan, EvidenceKind::tactical_motifs);
    }
  }
  if (plan.needs_theory) add_need(plan, EvidenceKind::theory);
  if (plan.needs_opening) add_need(plan, EvidenceKind::opening);
  if (plan.needs_profile) add_need(plan, EvidenceKind::user_profile);
  if (plan.engine_budget != EngineBudget::none) {
    add_need(plan, EvidenceKind::engine);
    add_need(plan, EvidenceKind::candidate_moves);
    add_need(plan, EvidenceKind::practicality);
    plan.needs_profile = true;
    add_need(plan, EvidenceKind::user_profile);
  }
  apply_context_model(plan, request, route, model);
  return plan;
}

}  // namespace kchess::ai
