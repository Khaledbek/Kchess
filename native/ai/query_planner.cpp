#include "query_planner.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>

#include "query_classifier.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Intent policy
// -----------------------------------------------------------------------------

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

struct AnalysisModeResolution {
  PositionAnalysisMode mode{PositionAnalysisMode::none};
  AnalysisModeSource source{AnalysisModeSource::none};
  bool explicit_current_turn{false};
  bool planner_override_allowed{true};
};

bool contains_any_phrase(std::string_view text,
                         std::initializer_list<std::string_view> phrases) {
  return std::any_of(phrases.begin(), phrases.end(), [&](std::string_view phrase) {
    return text.find(phrase) != std::string_view::npos;
  });
}

AnalysisModeResolution resolve_analysis_mode(const CoachRequest& request,
                                             CoachIntent effective_intent,
                                             bool needs_position) {
  if (!needs_position) return {};

  std::string text = request.user_text;
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  });

  // A native verdict challenge rechecks the previous authoritative judgment
  // from the same root board. It is not a new learner attempt.
  if (request.verdict_challenge.has_value()) {
    return {request.verdict_challenge->challenged_move_uci.has_value()
                ? PositionAnalysisMode::explain_move
                : PositionAnalysisMode::position_overview,
            AnalysisModeSource::native_state, true, false};
  }

  // Native completed-move state is stronger than conversational teaching state:
  // this turn is about evaluating the move the learner actually played.
  if (request.user_move_uci.has_value()) {
    return {PositionAnalysisMode::learner_move_evaluation,
            AnalysisModeSource::native_state, true, false};
  }

  if (contains_any_phrase(text, {
          "fastest loss", "lose fastest", "loses fastest", "quickest loss",
          "schnellster verlust", "schneller verlust", "schnell verlieren",
          "am schnellsten verlieren", "schnellsten matt", "schnellstes matt"})) {
    return {PositionAnalysisMode::fastest_loss,
            AnalysisModeSource::legacy_text_fallback, true, true};
  }
  if (contains_any_phrase(text, {
          "worst move", "worse move", "schlechteste zug", "schlechtester zug",
          "schlechtesten zug", "schlimmste zug", "schlimmster zug"})) {
    return {PositionAnalysisMode::worst_move,
            AnalysisModeSource::legacy_text_fallback, true, true};
  }
  const bool avoid_wording = contains_any_phrase(text, {
      "avoid", "prevent", "verhindern", "verhindere", "vermeiden", "vermeide"});
  const bool trade_wording = contains_any_phrase(text, {
      "trade", "exchange", "queen trade", "piece trade", "tausch", "abtausch",
      "damentausch", "damen tauschen", "figuren tauschen"});
  if (avoid_wording && trade_wording) {
    return {PositionAnalysisMode::avoid_trade,
            AnalysisModeSource::legacy_text_fallback, true, true};
  }
  if (contains_any_phrase(text, {
          "what is the threat", "what threatens", "opponent threat",
          "was droht", "welche drohung", "gegner droht", "droht mir"})) {
    return {PositionAnalysisMode::threat,
            AnalysisModeSource::legacy_text_fallback, true, true};
  }
  if (contains_any_phrase(text, {
          "what if", "if i play", "if i played", "was wenn", "wenn ich spiele",
          "wenn ich den zug", "was passiert nach"})) {
    return {PositionAnalysisMode::what_if_move,
            AnalysisModeSource::legacy_text_fallback, true, true};
  }
  if (request.mode == CoachMode::compare) {
    return {PositionAnalysisMode::compare_candidates,
            AnalysisModeSource::request_mode, true, false};
  }
  if (contains_any_phrase(text, {"compare", "comparison", "vergleich", "besser als",
                                 "schlechter als", "which of these"})) {
    return {PositionAnalysisMode::compare_candidates,
            AnalysisModeSource::legacy_text_fallback, true, true};
  }
  if (contains_any_phrase(text, {
          "best move", "best candidate", "what should i play", "which move should i play",
          "what move should i play", "suggest a move", "bester zug", "beste zug",
          "besten zug", "welchen zug soll ich", "welchen zug würdest du",
          "schlage einen zug vor"})) {
    return {PositionAnalysisMode::best_move,
            AnalysisModeSource::legacy_text_fallback, true, true};
  }
  if (request.hint_move_uci.has_value()) {
    return {PositionAnalysisMode::explain_move,
            AnalysisModeSource::native_state, true, false};
  }
  if (effective_intent == CoachIntent::move_explanation) {
    return {PositionAnalysisMode::explain_move,
            AnalysisModeSource::intent_default, !text.empty(), true};
  }
  return {PositionAnalysisMode::position_overview,
          AnalysisModeSource::intent_default, false, true};
}

EngineBudget choose_engine_budget(const CoachRequest& request, CoachIntent intent,
                                  PositionAnalysisMode analysis_mode) {
  if (!request.position_fen && !request.game_pgn) return EngineBudget::none;

  // Update 200: do not re-classify natural language here. Query planning owns
  // semantic routing once; budget policy consumes the resolved mode. This
  // removes a second, divergent phrase table that previously made native
  // routing more brittle and harder to maintain.
  if (request.mode == CoachMode::hint || request.mode == CoachMode::quiz) {
    return EngineBudget::probe;
  }
  switch (analysis_mode) {
    case PositionAnalysisMode::best_move:
    case PositionAnalysisMode::worst_move:
    case PositionAnalysisMode::fastest_loss:
    case PositionAnalysisMode::avoid_trade:
    case PositionAnalysisMode::threat:
    case PositionAnalysisMode::explain_move:
    case PositionAnalysisMode::what_if_move:
    case PositionAnalysisMode::compare_candidates:
    case PositionAnalysisMode::learner_move_evaluation:
      return EngineBudget::probe;
    case PositionAnalysisMode::none:
    case PositionAnalysisMode::position_overview:
      break;
  }
  if (intent == CoachIntent::move_explanation || intent == CoachIntent::tactic ||
      intent == CoachIntent::game_review) {
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

template <typename T>
void add_unique(std::vector<T>& values, T value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void seed_evidence_plan(QueryPlan& plan, const CoachRequest& request) {
  auto& evidence_plan = plan.evidence_plan;
  evidence_plan.confidence = 0.0;
  evidence_plan.freshness = EvidenceFreshness::reuse_first;
  evidence_plan.elo_target = EvidenceEloTarget::none;
  switch (request.mode) {
    case CoachMode::compare:
      evidence_plan.interaction = EvidenceInteraction::compare;
      break;
    case CoachMode::quiz:
    case CoachMode::hint:
    case CoachMode::teach:
      evidence_plan.interaction = EvidenceInteraction::teach;
      break;
    case CoachMode::review:
      evidence_plan.interaction = EvidenceInteraction::review;
      break;
    case CoachMode::explain:
    case CoachMode::answer:
    case CoachMode::plan:
    default:
      evidence_plan.interaction = EvidenceInteraction::explain;
      break;
  }
  evidence_plan.priority = request.mode == CoachMode::hint ||
                                   request.mode == CoachMode::quiz
                               ? EvidencePlanPriority::foreground
                               : EvidencePlanPriority::foreground;
  evidence_plan.depth =
      plan.response_depth == ResponseDepth::detailed
          ? EvidenceAnalysisDepth::deep
          : (plan.response_depth == ResponseDepth::concise
                 ? EvidenceAnalysisDepth::shallow
                 : EvidenceAnalysisDepth::medium);
  evidence_plan.scope = plan.needs_position
                            ? EvidenceAnalysisScope::whole_position
                            : EvidenceAnalysisScope::none;

  if (plan.has_conversation_context) {
    add_unique(evidence_plan.sources, EvidenceSource::conversation);
    add_unique(evidence_plan.needs, EvidenceNeed::conversation_reference);
  }
  if (plan.needs_position) {
    add_unique(evidence_plan.sources, EvidenceSource::existing_analysis);
    add_unique(evidence_plan.sources, EvidenceSource::position_features);
    add_unique(evidence_plan.needs, EvidenceNeed::position_structure);
    add_unique(evidence_plan.needs, EvidenceNeed::piece_activity);
  }
  if (plan.needs_concepts) {
    add_unique(evidence_plan.sources, EvidenceSource::chess_concepts);
    add_unique(evidence_plan.needs, EvidenceNeed::concept_explanation);
  }
  if (plan.needs_opening || plan.needs_theory) {
    add_unique(evidence_plan.sources, EvidenceSource::opening_knowledge);
    add_unique(evidence_plan.needs, EvidenceNeed::opening_context);
  }
  if (plan.needs_profile) {
    add_unique(evidence_plan.sources, EvidenceSource::player_profile);
    add_unique(evidence_plan.needs, EvidenceNeed::player_tendencies);
  }
  if (plan.engine_budget != EngineBudget::none) {
    add_unique(evidence_plan.sources, EvidenceSource::engine);
    add_unique(evidence_plan.needs, EvidenceNeed::candidate_moves);
    add_unique(evidence_plan.needs, EvidenceNeed::move_evaluations);
    add_unique(evidence_plan.needs, EvidenceNeed::best_reply);
  }

  switch (plan.analysis_mode) {
    case PositionAnalysisMode::best_move:
      add_unique(evidence_plan.needs, EvidenceNeed::candidate_moves);
      evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
      break;
    case PositionAnalysisMode::worst_move:
    case PositionAnalysisMode::fastest_loss:
      add_unique(evidence_plan.needs, EvidenceNeed::legal_moves);
      add_unique(evidence_plan.needs, EvidenceNeed::move_evaluations);
      add_unique(evidence_plan.needs, EvidenceNeed::principal_variation);
      add_unique(evidence_plan.needs, EvidenceNeed::material_consequences);
      add_unique(evidence_plan.needs, EvidenceNeed::forced_mate);
      evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
      break;
    case PositionAnalysisMode::avoid_trade:
      add_unique(evidence_plan.needs, EvidenceNeed::legal_moves);
      add_unique(evidence_plan.needs, EvidenceNeed::move_comparison);
      evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
      break;
    case PositionAnalysisMode::threat:
      add_unique(evidence_plan.sources, EvidenceSource::tactical_detector);
      add_unique(evidence_plan.needs, EvidenceNeed::threats);
      add_unique(evidence_plan.needs, EvidenceNeed::tactical_motifs);
      break;
    case PositionAnalysisMode::compare_candidates:
      add_unique(evidence_plan.needs, EvidenceNeed::move_comparison);
      add_unique(evidence_plan.needs, EvidenceNeed::principal_variation);
      evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
      break;
    case PositionAnalysisMode::explain_move:
    case PositionAnalysisMode::what_if_move:
    case PositionAnalysisMode::learner_move_evaluation:
      add_unique(evidence_plan.needs, EvidenceNeed::principal_variation);
      add_unique(evidence_plan.needs, EvidenceNeed::material_consequences);
      evidence_plan.scope = EvidenceAnalysisScope::single_move;
      break;
    case PositionAnalysisMode::none:
    case PositionAnalysisMode::position_overview:
      break;
  }
}

bool model_extra_allowed(EvidenceKind kind, const QueryPlan& plan) {
  switch (kind) {
    case EvidenceKind::chess_concepts:
    case EvidenceKind::theory:
    case EvidenceKind::opening:
    case EvidenceKind::user_profile:
      return kind != EvidenceKind::user_profile || plan.needs_profile;
    case EvidenceKind::position_features:
    case EvidenceKind::position_weaknesses:
    case EvidenceKind::weakness_exploitation:
    case EvidenceKind::tactical_motifs:
    case EvidenceKind::strategic_plans:
      return plan.needs_position;
    default:
      return false;
  }
}

bool has_need(const EvidencePlan& plan, EvidenceNeed need) {
  return std::find(plan.needs.begin(), plan.needs.end(), need) != plan.needs.end();
}

bool has_source(const EvidencePlan& plan, EvidenceSource source) {
  return std::find(plan.sources.begin(), plan.sources.end(), source) != plan.sources.end();
}

bool board_source(EvidenceSource source) {
  switch (source) {
    case EvidenceSource::existing_analysis:
    case EvidenceSource::engine:
    case EvidenceSource::human_model:
    case EvidenceSource::position_features:
    case EvidenceSource::tactical_detector:
    case EvidenceSource::opening_knowledge:
      return true;
    case EvidenceSource::player_profile:
    case EvidenceSource::game_history:
    case EvidenceSource::conversation:
    case EvidenceSource::chess_concepts:
      return false;
  }
  return false;
}

void add_legacy_evidence_for_source(QueryPlan& plan, EvidenceSource source) {
  switch (source) {
    case EvidenceSource::existing_analysis:
      add_need(plan, EvidenceKind::existing_analysis);
      break;
    case EvidenceSource::engine:
      add_need(plan, EvidenceKind::engine);
      add_need(plan, EvidenceKind::candidate_moves);
      add_need(plan, EvidenceKind::practicality);
      break;
    case EvidenceSource::human_model:
      add_need(plan, EvidenceKind::human_model);
      break;
    case EvidenceSource::position_features:
      add_need(plan, EvidenceKind::position_features);
      if (has_need(plan.evidence_plan, EvidenceNeed::position_structure)) {
        add_need(plan, EvidenceKind::position_weaknesses);
      }
      if (has_need(plan.evidence_plan, EvidenceNeed::strategic_plans)) {
        add_need(plan, EvidenceKind::strategic_plans);
        add_need(plan, EvidenceKind::weakness_exploitation);
      }
      break;
    case EvidenceSource::tactical_detector:
      add_need(plan, EvidenceKind::tactical_motifs);
      break;
    case EvidenceSource::opening_knowledge:
      add_need(plan, EvidenceKind::opening);
      add_need(plan, EvidenceKind::theory);
      break;
    case EvidenceSource::player_profile:
    case EvidenceSource::game_history:
      add_need(plan, EvidenceKind::user_profile);
      break;
    case EvidenceSource::conversation:
      add_need(plan, EvidenceKind::conversation);
      break;
    case EvidenceSource::chess_concepts:
      add_need(plan, EvidenceKind::chess_concepts);
      break;
  }
}

PositionAnalysisMode evidence_plan_analysis_mode(const EvidencePlan& evidence_plan,
                                           PositionAnalysisMode fallback) {
  if (evidence_plan.scope == EvidenceAnalysisScope::all_legal_moves) {
    if (has_need(evidence_plan, EvidenceNeed::forced_mate)) {
      return PositionAnalysisMode::fastest_loss;
    }
    if (has_need(evidence_plan, EvidenceNeed::move_evaluations)) {
      return PositionAnalysisMode::worst_move;
    }
  }
  if (has_need(evidence_plan, EvidenceNeed::threats)) {
    return PositionAnalysisMode::threat;
  }
  if (has_need(evidence_plan, EvidenceNeed::move_comparison)) {
    return PositionAnalysisMode::compare_candidates;
  }
  if (evidence_plan.scope == EvidenceAnalysisScope::single_move &&
      (has_need(evidence_plan, EvidenceNeed::principal_variation) ||
       has_need(evidence_plan, EvidenceNeed::move_evaluations))) {
    return PositionAnalysisMode::explain_move;
  }
  if ((evidence_plan.scope == EvidenceAnalysisScope::multiple_moves ||
       evidence_plan.scope == EvidenceAnalysisScope::whole_position) &&
      has_need(evidence_plan, EvidenceNeed::candidate_moves) &&
      has_need(evidence_plan, EvidenceNeed::move_evaluations)) {
    return PositionAnalysisMode::best_move;
  }
  return fallback;
}


}  // namespace

// -----------------------------------------------------------------------------
// Section: Public planning
// -----------------------------------------------------------------------------

QueryPlan QueryPlanner::plan(const CoachRequest& request,
                             const DomainRoute& route) const {
  QueryPlan plan;
  plan.intent = route.intent;
  plan.context_intent = route.context_intent;
  plan.routing_confidence = route.confidence;
  plan.response_depth = choose_depth(request, route.intent);
  // Update 210: conversational continuity is independent from intent
  // inheritance. A concrete current question keeps its newly detected intent
  // while still receiving the compact previous-turn goal/answer when a session
  // exists. Only an elliptical follow-up uses context_intent as the effective
  // intent below; previous answer prose remains non-authoritative evidence.
  // A valid session can carry useful conversational continuity even when the
  // router does not need to inherit an intent. ContextBuilder will emit an
  // empty summary when the session has no remembered turn, so enabling the
  // source here is safe and avoids dropping explicit follow-up context merely
  // because context_intent is intentionally unknown.
  plan.has_conversation_context = request.session_id.has_value();

  if (!route.chess_domain || route.intent == CoachIntent::off_topic ||
      route.intent == CoachIntent::unknown) {
    return plan;
  }

  const CoachIntent effective_intent =
      route.intent == CoachIntent::follow_up &&
              route.context_intent != CoachIntent::unknown
          ? route.context_intent
          : route.intent;
  const auto classification = CoachQueryClassifier{}.classify(request, route);
  plan.query_family = classification.family;
  plan.profile_scope = classification.profile_scope;
  plan.needs_position = classification.needs_position ||
      request.verdict_challenge.has_value();
  plan.needs_profile = classification.needs_profile;
  plan.needs_concepts = classification.needs_concepts || concepts_relevant(effective_intent);
  plan.needs_theory = effective_intent == CoachIntent::opening;
  plan.needs_opening = effective_intent == CoachIntent::opening;
  const auto analysis_mode =
      resolve_analysis_mode(request, effective_intent, plan.needs_position);
  plan.analysis_mode = analysis_mode.mode;
  plan.analysis_mode_source = analysis_mode.source;
  plan.analysis_mode_explicit = analysis_mode.explicit_current_turn;
  plan.engine_budget = (classification.may_need_engine ||
                        request.verdict_challenge.has_value())
      ? choose_engine_budget(request, effective_intent, plan.analysis_mode)
      : EngineBudget::none;
  if (request.verdict_challenge.has_value() &&
      plan.engine_budget == EngineBudget::none) {
    plan.engine_budget = EngineBudget::probe;
  }
  // A concrete native analysis mode always requires objective engine grounding.
  // The next pipeline stages may specialize the actual work, but no provider may
  // answer a requested best/worst/threat/etc. question from prose alone.
  if (classification.may_need_engine &&
      plan.analysis_mode != PositionAnalysisMode::none &&
      plan.analysis_mode != PositionAnalysisMode::position_overview) {
    plan.engine_budget = std::max(plan.engine_budget, EngineBudget::probe);
  }

  add_need(plan, EvidenceKind::cache);
  if (plan.has_conversation_context) add_need(plan, EvidenceKind::conversation);
  if (plan.needs_concepts) {
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
    // Engine grounding and personal-profile context are independent. Position
    // questions may use objective practicality without exposing personal chunks
    // unless the classifier/request actually requires them.
    if (plan.needs_profile) add_need(plan, EvidenceKind::user_profile);
  }
  seed_evidence_plan(plan, request);
  if (request.verdict_challenge.has_value()) {
    // User disagreement is a request for objective re-verification. Never let
    // conversation/cache reuse stand in for a fresh native check.
    plan.evidence_plan.freshness = EvidenceFreshness::fresh_required;
    plan.evidence_plan.priority = EvidencePlanPriority::foreground;
    if (request.verdict_challenge->challenged_move_uci.has_value()) {
      plan.evidence_plan.scope = EvidenceAnalysisScope::single_move;
      add_unique(plan.evidence_plan.needs, EvidenceNeed::candidate_moves);
      add_unique(plan.evidence_plan.needs, EvidenceNeed::move_evaluations);
      add_unique(plan.evidence_plan.needs, EvidenceNeed::principal_variation);
    }
  }
  return plan;
}

}  // namespace kchess::ai
