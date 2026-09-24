#include "coach_orchestrator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "context_builder.h"
#include "domain_router.h"
#include "dto/evidence.h"
#include "dto/query_plan.h"
#include "evidence_retriever.h"
#include "experts/context_evidence_experts.h"
#include "experts/evidence_aggregator.h"
#include "experts/human_elo_bot_expert.h"
#include "interaction/action_catalog.h"
#include "interaction/action_chain_engine.h"
#include "interaction/action_fulfillment_validator.h"
#include "interaction/answer_gate.h"
#include "interaction/conversation_state.h"
#include "interaction/interaction_requirement_resolver.h"
#include "interaction/parameter_parser.h"
#include "interaction/primary_interaction_planner.h"
#include "position/move_contrast.h"
#include "position/position_analysis_stage.h"
#include "practicality/player_practicality.h"
#include "optimization/provider_input_optimizer.h"
#include "optimization/validated_response_cache.h"
#include "practicality/practicality_engine.h"
#include "providers/llm_provider.h"
#include "query_planner.h"
#include "rendering/verified_fact_renderer.h"
#include "teaching/teaching_planner.h"
#include "validation/response_validator.h"
#include "verdict/chess_verdict_builder.h"
#include "verdict/chess_verdict_review.h"

namespace kchess::ai {
namespace {


interaction::ConversationState interaction_state_for_request(
    const CoachRequest& request) {
  interaction::ConversationState state;
  if (request.position_fen) state.position_key = request.position_fen;
  if (request.context_id) state.game_id = request.context_id;
  if (request.user_move_uci) state.active_move_uci = request.user_move_uci;
  if (request.native_context_state.user_rating) {
    state.rating_target = request.native_context_state.user_rating;
  }
  state.interaction_mode = "coach";
  return state;
}

template <typename T>
void append_unique(std::vector<T>& values, T value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void require_native_engine_grounding(QueryPlan& plan,
                                     EvidenceAnalysisScope scope) {
  plan.needs_position = true;
  plan.engine_budget = std::max(plan.engine_budget, EngineBudget::probe);
  append_unique(plan.evidence, EvidenceKind::existing_analysis);
  append_unique(plan.evidence, EvidenceKind::engine);
  append_unique(plan.evidence, EvidenceKind::candidate_moves);
  append_unique(plan.evidence_plan.sources, EvidenceSource::existing_analysis);
  append_unique(plan.evidence_plan.sources, EvidenceSource::engine);
  append_unique(plan.evidence_plan.needs, EvidenceNeed::candidate_moves);
  append_unique(plan.evidence_plan.needs, EvidenceNeed::move_evaluations);
  append_unique(plan.evidence_plan.needs, EvidenceNeed::best_reply);
  plan.evidence_plan.scope = scope;
  plan.evidence_plan.priority = EvidencePlanPriority::foreground;
  if (plan.evidence_plan.freshness == EvidenceFreshness::reuse_only) {
    plan.evidence_plan.freshness = EvidenceFreshness::fresh_if_missing;
  }
}

bool interaction_has_semantic(
    const interaction::ResolvedInteractionPlan& plan,
    const std::string_view semantic_id) {
  return std::find(plan.semantic_ids.begin(), plan.semantic_ids.end(),
                   semantic_id) != plan.semantic_ids.end();
}

enum class VerdictGroundingRequirement {
  none,
  position,
  move,
};

VerdictGroundingRequirement verdict_grounding_requirement(
    const interaction::ResolvedInteractionPlan& interaction_plan,
    const std::optional<ChessVerdictChallenge>& challenge) {
  if (challenge && challenge->challenged_move_uci.has_value()) {
    return VerdictGroundingRequirement::move;
  }
  if (interaction_has_semantic(interaction_plan, "evaluate_move") ||
      interaction_has_semantic(interaction_plan, "find_blunder") ||
      interaction_has_semantic(interaction_plan, "find_mistake")) {
    return VerdictGroundingRequirement::move;
  }
  if (interaction_has_semantic(interaction_plan, "evaluate_position")) {
    return VerdictGroundingRequirement::position;
  }
  return VerdictGroundingRequirement::none;
}

bool verdict_grounding_available(
    const VerdictGroundingRequirement requirement,
    const std::optional<ChessVerdictContract>& verdict) {
  if (requirement == VerdictGroundingRequirement::none) return true;
  if (!verdict || !verdict->authoritative) return false;
  if (requirement == VerdictGroundingRequirement::move) {
    return verdict->evaluated_move_uci.has_value() &&
        verdict->move_verdict != MoveVerdict::unknown;
  }
  return verdict->position_verdict != PositionVerdict::unknown;
}

void apply_primary_interaction_constraints(
    const interaction::ResolvedInteractionPlan& interaction_plan,
    QueryPlan& plan) {
  // The interaction layer owns semantic intent. Legacy QueryPlanner remains an
  // evidence-budget adapter during migration, but it may not overwrite these
  // high-confidence action semantics.
  if (interaction_plan.request_kind == interaction::InteractionRequestKind::app_action ||
      interaction_plan.request_kind ==
          interaction::InteractionRequestKind::conversation_control) {
    // Product/session actions are not chess-analysis requests. In particular, a
    // start-position FEN must not force a direct "start game" utterance through
    // position_overview, engine/evidence retrieval, or positional teaching.
    plan.query_family = QueryFamily::unknown;
    plan.intent = interaction_plan.answer_intent ==
                          interaction::InteractionAnswerIntent::coach
                      ? CoachIntent::training
                      : CoachIntent::unknown;
    plan.engine_budget = EngineBudget::none;
    plan.analysis_mode = PositionAnalysisMode::none;
    plan.analysis_mode_source = AnalysisModeSource::none;
    plan.analysis_mode_explicit = true;
    plan.needs_position = false;
    plan.needs_profile = false;
    plan.needs_concepts = false;
    plan.needs_theory = false;
    plan.needs_opening = false;
    plan.evidence.clear();
    plan.evidence_plan.sources.clear();
    plan.evidence_plan.needs.clear();
    plan.evidence_plan.scope = EvidenceAnalysisScope::none;
    plan.evidence_plan.confidence = 1.0;
  }
  if (interaction_plan.has_action(interaction::PrimitiveAction::analyze_position)) {
    require_native_engine_grounding(plan, EvidenceAnalysisScope::whole_position);
    if (interaction_has_semantic(interaction_plan, "find_worst")) {
      plan.analysis_mode = PositionAnalysisMode::worst_move;
      plan.analysis_mode_explicit = true;
      plan.evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
    } else if (interaction_has_semantic(interaction_plan, "find_fastest_loss")) {
      plan.analysis_mode = PositionAnalysisMode::fastest_loss;
      plan.analysis_mode_explicit = true;
      plan.evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
    } else if (interaction_has_semantic(interaction_plan, "find_best")) {
      plan.analysis_mode = PositionAnalysisMode::best_move;
      plan.analysis_mode_explicit = true;
      plan.evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
    } else if (interaction_has_semantic(interaction_plan, "compare")) {
      plan.analysis_mode = PositionAnalysisMode::compare_candidates;
      plan.analysis_mode_explicit = true;
      plan.evidence_plan.scope = EvidenceAnalysisScope::multiple_moves;
    } else if (interaction_has_semantic(interaction_plan, "evaluate_position")) {
      // Position judgment is still an explicit engine-backed analysis even
      // though the semantic mode remains a whole-position overview.
      plan.analysis_mode = PositionAnalysisMode::position_overview;
      plan.analysis_mode_explicit = true;
    }
  }
  if (interaction_plan.has_action(interaction::PrimitiveAction::analyze_move)) {
    plan.analysis_mode = PositionAnalysisMode::explain_move;
    plan.analysis_mode_explicit = true;
    require_native_engine_grounding(plan, EvidenceAnalysisScope::single_move);
    append_unique(plan.evidence_plan.needs, EvidenceNeed::principal_variation);
    append_unique(plan.evidence_plan.needs, EvidenceNeed::material_consequences);
  }
}

std::string_view diagnostic_intent_name(const CoachIntent intent) {
  switch (intent) {
    case CoachIntent::position: return "position";
    case CoachIntent::move_explanation: return "move_explanation";
    case CoachIntent::plan: return "plan";
    case CoachIntent::tactic: return "tactic";
    case CoachIntent::opening: return "opening";
    case CoachIntent::endgame: return "endgame";
    case CoachIntent::chess_concept: return "chess_concept";
    case CoachIntent::chess_rules: return "chess_rules";
    case CoachIntent::chess_history: return "chess_history";
    case CoachIntent::player_development: return "player_development";
    case CoachIntent::game_review: return "game_review";
    case CoachIntent::training: return "training";
    case CoachIntent::follow_up: return "follow_up";
    case CoachIntent::off_topic: return "off_topic";
    case CoachIntent::unknown:
    default: return "unknown";
  }
}

std::string_view diagnostic_analysis_mode_name(const PositionAnalysisMode mode) {
  switch (mode) {
    case PositionAnalysisMode::position_overview: return "position_overview";
    case PositionAnalysisMode::best_move: return "best_move";
    case PositionAnalysisMode::worst_move: return "worst_move";
    case PositionAnalysisMode::fastest_loss: return "fastest_loss";
    case PositionAnalysisMode::avoid_trade: return "avoid_trade";
    case PositionAnalysisMode::threat: return "threat";
    case PositionAnalysisMode::explain_move: return "explain_move";
    case PositionAnalysisMode::what_if_move: return "what_if_move";
    case PositionAnalysisMode::compare_candidates: return "compare_candidates";
    case PositionAnalysisMode::learner_move_evaluation: return "learner_move_evaluation";
    case PositionAnalysisMode::none:
    default: return "none";
  }
}

std::string_view diagnostic_evidence_freshness_name(const EvidenceFreshness value) {
  switch (value) {
    case EvidenceFreshness::reuse_only: return "reuse_only";
    case EvidenceFreshness::reuse_first: return "reuse_first";
    case EvidenceFreshness::fresh_if_missing: return "fresh_if_missing";
    case EvidenceFreshness::fresh_required: return "fresh_required";
  }
  return "reuse_first";
}

std::string_view diagnostic_evidence_interaction_name(const EvidenceInteraction value) {
  switch (value) {
    case EvidenceInteraction::explain: return "explain";
    case EvidenceInteraction::compare: return "compare";
    case EvidenceInteraction::predict: return "predict";
    case EvidenceInteraction::search: return "search";
    case EvidenceInteraction::teach: return "teach";
    case EvidenceInteraction::review: return "review";
  }
  return "explain";
}

std::string_view diagnostic_evidence_elo_target_name(const EvidenceEloTarget value) {
  switch (value) {
    case EvidenceEloTarget::none: return "none";
    case EvidenceEloTarget::current_user: return "current_user";
    case EvidenceEloTarget::opponent: return "opponent";
    case EvidenceEloTarget::elo_800: return "800";
    case EvidenceEloTarget::elo_1200: return "1200";
    case EvidenceEloTarget::elo_1600: return "1600";
    case EvidenceEloTarget::elo_2000: return "2000";
    case EvidenceEloTarget::master: return "master";
  }
  return "none";
}


bool evidence_plan_requests_source(const EvidencePlan& plan,
                                   EvidenceSource source) {
  return std::find(plan.sources.begin(), plan.sources.end(), source) !=
         plan.sources.end();
}

void append_human_model_evidence(const QueryPlan& plan,
                                 const RetrievedEvidence& retrieved,
                                 std::vector<EvidenceItem>& evidence) {
  if (!evidence_plan_requests_source(plan.evidence_plan,
                                     EvidenceSource::human_model) ||
      !retrieved.candidate_moves.has_value()) {
    return;
  }
  std::optional<int> learner_elo;
  if (retrieved.practicality_player.has_value()) {
    learner_elo = retrieved.practicality_player->rating;
  }

  HumanEloBotExpert expert;
  EvidenceItem item;
  switch (plan.evidence_plan.elo_target) {
    case EvidenceEloTarget::current_user:
      item = learner_elo.has_value()
                 ? expert.evidence(*retrieved.candidate_moves, *learner_elo)
                 : expert.evidence_grid(*retrieved.candidate_moves, learner_elo);
      break;
    case EvidenceEloTarget::elo_800:
      item = expert.evidence(*retrieved.candidate_moves, 800);
      break;
    case EvidenceEloTarget::elo_1200:
      item = expert.evidence(*retrieved.candidate_moves, 1200);
      break;
    case EvidenceEloTarget::elo_1600:
      item = expert.evidence(*retrieved.candidate_moves, 1600);
      break;
    case EvidenceEloTarget::elo_2000:
      item = expert.evidence(*retrieved.candidate_moves, 2000);
      break;
    case EvidenceEloTarget::master:
      item = expert.evidence(*retrieved.candidate_moves, 2400);
      break;
    case EvidenceEloTarget::opponent:
      // No authoritative opponent rating is present in RetrievedEvidence yet.
      // Keep a grid instead of inventing one; a future profile/game adapter may
      // narrow this once such a rating is explicitly available.
      [[fallthrough]];
    case EvidenceEloTarget::none:
    default:
      item = expert.evidence_grid(*retrieved.candidate_moves, learner_elo);
      break;
  }
  if (item.confidence <= 0.0 || item.payload.empty()) return;
  const bool duplicate = std::any_of(
      evidence.begin(), evidence.end(), [&](const EvidenceItem& existing) {
        return existing.id == item.id && existing.payload == item.payload;
      });
  if (!duplicate) evidence.push_back(std::move(item));
}

struct ProviderEvidenceBasis {
  std::vector<EvidenceItem> items;
  EvidenceAggregationResult aggregation;
};

bool same_evidence_item(const EvidenceItem& left, const EvidenceItem& right) {
  return left.id == right.id && left.kind == right.kind &&
         left.payload == right.payload && left.confidence == right.confidence;
}

ProviderEvidenceBasis aggregate_provider_evidence(
    const QueryPlan& plan, const std::vector<EvidenceItem>& evidence) {
  ProviderEvidenceBasis output;
  const auto normalized = normalize_expert_evidence(evidence);
  output.aggregation = EvidenceAggregator{}.aggregate(
      plan.evidence_plan, normalized, 0);
  output.items.reserve(output.aggregation.items.size() + 4);
  for (const auto& item : output.aggregation.items) {
    output.items.push_back(item.evidence.item);
  }

  // The learned plan is advisory for information selection. Never let a low-
  // quality plan hide the native trust-boundary objects required for safe
  // rendering/fallback or the explicit session/profile context requested by
  // the current native turn.
  for (const auto& item : evidence) {
    const bool mandatory =
        item.kind == EvidenceKind::candidate_moves ||
        item.kind == EvidenceKind::move_contrast ||
        (plan.has_conversation_context &&
         item.kind == EvidenceKind::conversation) ||
        (plan.needs_profile && item.kind == EvidenceKind::user_profile);
    if (!mandatory) continue;
    if (std::none_of(output.items.begin(), output.items.end(),
                     [&](const EvidenceItem& existing) {
                       return same_evidence_item(existing, item);
                     })) {
      output.items.push_back(item);
    }
  }

  // A malformed/over-restrictive planner result must degrade to the complete
  // native evidence set rather than leaving the provider blind.
  if (output.items.empty()) output.items = evidence;
  return output;
}

struct NativeEvidenceSummary {
  std::size_t candidates{0};
  std::size_t facts{0};
  std::string focus_kind;
};

NativeEvidenceSummary summarize_native_candidate_evidence(
    const std::vector<EvidenceItem>& evidence) {
  NativeEvidenceSummary summary;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = nlohmann::json::parse(item.payload, nullptr, false);
    if (!payload.is_object()) continue;
    const auto count_candidate = [&](const char* key) {
      if (payload.contains(key) && payload[key].is_object() &&
          !payload[key].value("move_uci", "").empty()) {
        ++summary.candidates;
      }
    };
    count_candidate("best");
    count_candidate("focus");
    count_candidate("user_move");
    if (payload.contains("alternatives") && payload["alternatives"].is_array()) {
      for (const auto& candidate : payload["alternatives"])
        if (candidate.is_object() && !candidate.value("move_uci", "").empty())
          ++summary.candidates;
    }
    if (payload.contains("facts") && payload["facts"].is_array())
      summary.facts += payload["facts"].size();
    if (summary.focus_kind.empty())
      summary.focus_kind = payload.value("focus_kind", "");
  }
  return summary;
}


// -----------------------------------------------------------------------------
// Section: Practicality stage
// -----------------------------------------------------------------------------

void append_practicality(
    const QueryPlan& plan,
    const std::optional<CandidateMoveSet>& candidates,
    const std::optional<PracticalityPlayerContext>& player,
    const PositionAnalysisArtifacts& artifacts,
    std::vector<EvidenceItem>& evidence) {
  if (std::find(plan.evidence.begin(), plan.evidence.end(),
                EvidenceKind::practicality) == plan.evidence.end() ||
      !candidates.has_value() || !candidates->best.has_value()) {
    return;
  }

  PracticalityEngine practicality;
  const auto assessment =
      practicality.assess(*candidates, artifacts.features, artifacts.tactics);
  evidence.push_back(practicality.evidence(assessment));

  if (player.has_value()) {
    PlayerPracticalityEngine player_practicality;
    const auto adjusted =
        player_practicality.assess(*candidates, assessment, *player);
    if (adjusted.confidence > 0.0) {
      evidence.push_back(player_practicality.evidence(adjusted));
    }
  }
}

// -----------------------------------------------------------------------------
// Section: Provider and validation stage
// -----------------------------------------------------------------------------

struct ProviderStageResult {
  std::optional<StructuredCoachContent> content;
  std::vector<std::string> validation_issues;
  std::string provider_error_code;
  std::string safe_fallback_kind;
  std::string fallback_reason;
  bool validation_passed{true};
  bool repaired{false};
  bool response_cache_requested{false};
  bool response_cache_hit{false};
  std::uint64_t request_build_ms{0};
  std::uint64_t response_cache_key_ms{0};
  std::uint64_t provider_ms{0};
  std::uint64_t validation_ms{0};
  std::uint64_t repair_provider_ms{0};
  std::size_t provider_evidence_count{0};
  std::size_t provider_evidence_input_count{0};
  std::size_t provider_exact_duplicates_dropped{0};
  std::size_t provider_compacted_items{0};
  std::size_t provider_compaction_savings_tokens{0};
  std::size_t provider_estimated_input_tokens{0};
  std::size_t provider_estimated_selected_tokens{0};
  std::size_t provider_evidence_budget_tokens{0};
  int provider_calls{0};
};

std::vector<CoachRecommendation> native_candidate_recommendations(
    const std::vector<EvidenceItem>& evidence, const std::size_t limit) {
  std::vector<CoachRecommendation> result;
  if (limit == 0) return result;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = nlohmann::json::parse(item.payload, nullptr, false);
    if (!payload.is_object()) continue;
    const auto append = [&](const nlohmann::json& candidate) {
      if (!candidate.is_object() || result.size() >= limit) return;
      const auto candidate_id = candidate.value("candidate_id", "");
      const auto move_uci = candidate.value("move_uci", "");
      if (candidate_id.empty() || move_uci.empty()) return;
      if (std::any_of(result.begin(), result.end(), [&](const auto& existing) {
            return existing.move_uci == move_uci;
          })) {
        return;
      }
      result.push_back({
          .candidate_id = candidate_id,
          .move_uci = move_uci,
          .text = {},
          .evidence_ids = {item.id},
          .confidence = item.confidence,
      });
    };
    if (payload.contains("best")) append(payload["best"]);
    if (payload.contains("alternatives") && payload["alternatives"].is_array()) {
      for (const auto& candidate : payload["alternatives"]) append(candidate);
    }
    if (!result.empty()) break;
  }
  return result;
}

bool has_visible_quiz_candidate(const std::vector<EvidenceItem>& evidence) {
  return !native_candidate_recommendations(evidence, 1).empty();
}

std::vector<std::string> native_fallback_board_moves(
    const std::vector<EvidenceItem>& evidence,
    const std::string_view fallback_kind,
    const std::size_t limit) {
  std::vector<std::string> result;
  if (limit == 0) return result;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = nlohmann::json::parse(item.payload, nullptr, false);
    if (!payload.is_object()) continue;
    const auto append = [&](const nlohmann::json& candidate) {
      if (!candidate.is_object() || result.size() >= limit) return;
      const auto move = candidate.value("move_uci", "");
      if (move.empty() || std::find(result.begin(), result.end(), move) != result.end())
        return;
      result.push_back(move);
    };
    if (fallback_kind == "worst_move_analysis" ||
        fallback_kind == "fastest_loss_analysis") {
      if (payload.value("focus_kind", "") ==
              (fallback_kind == "fastest_loss_analysis" ? "fastest_loss" : "worst_move") &&
          payload.contains("focus")) {
        append(payload["focus"]);
      }
      break;
    }
    if (payload.contains("best")) append(payload["best"]);
    if (fallback_kind == "candidate_comparison" &&
        payload.contains("alternatives") && payload["alternatives"].is_array()) {
      for (const auto& candidate : payload["alternatives"]) append(candidate);
    }
    if (!result.empty()) break;
  }
  return result;
}

std::string safe_fallback_kind(const CoachRequest& request,
                               const std::vector<EvidenceItem>& visible_evidence,
                               const QueryPlan& plan) {
  if (request.mode == CoachMode::quiz &&
      has_visible_quiz_candidate(visible_evidence)) {
    return "quiz_question";
  }
  if (request.mode == CoachMode::hint && request.position_fen.has_value()) {
    return "hint_prompt";
  }
  if (!plan.needs_position || !request.position_fen.has_value()) return {};

  if (plan.analysis_mode == PositionAnalysisMode::worst_move &&
      !native_fallback_board_moves(visible_evidence, "worst_move_analysis", 1).empty()) {
    return "worst_move_analysis";
  }
  if (plan.analysis_mode == PositionAnalysisMode::fastest_loss &&
      !native_fallback_board_moves(visible_evidence, "fastest_loss_analysis", 1).empty()) {
    return "fastest_loss_analysis";
  }

  const auto candidates = native_candidate_recommendations(visible_evidence, 2);
  if (request.mode == CoachMode::compare && candidates.size() >= 2) {
    return "candidate_comparison";
  }
  if (!candidates.empty() &&
      (request.mode == CoachMode::answer || request.mode == CoachMode::explain ||
       request.mode == CoachMode::plan || request.mode == CoachMode::review ||
       request.mode == CoachMode::teach || request.mode == CoachMode::compare)) {
    return "position_explanation";
  }
  return {};
}

bool has_non_repairable_authority_issue(
    const std::vector<std::string>& issues) {
  static constexpr std::string_view kNonRepairable[] = {
      "claim_move_illegal",
      "recommendation_move_illegal",
      "claim_candidate_reference_invalid",
      "recommendation_candidate_reference_invalid",
      "teaching_move_revealed_by_recommendation",
      "verified_fact_token_not_grounded",
      "verified_candidate_token_not_grounded",
      "verified_fact_token_unknown",
      "verified_recommendation_token_mismatch",
      "verified_recommendation_token_unknown",
  };
  return std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
    return std::any_of(std::begin(kNonRepairable), std::end(kNonRepairable),
                       [&](const auto expected) { return issue == expected; });
  });
}

enum class RepairDecision {
  attempt,
  skip_empty,
  skip_automatic,
  skip_native_fallback,
  skip_authority,
};

RepairDecision repair_decision(const CoachRequest& request,
                               const std::vector<std::string>& issues,
                               const std::string_view fallback_kind) {
  if (issues.empty()) return RepairDecision::skip_empty;

  // Automatic coaching is opportunistic and must never double provider cost or
  // latency for a prose/schema repair. The next board event can request fresh
  // grounded output, while deterministic native fallback remains available
  // when the current position supports one.
  if (request.automatic_turn) return RepairDecision::skip_automatic;

  // Once native code can answer the current board request safely, prefer that
  // deterministic path over a second remote generation. This keeps invalid LLM
  // prose from turning a one-call user question into a two-call request.
  if (!fallback_kind.empty()) return RepairDecision::skip_native_fallback;

  // A provider answer that crossed the native move/fact authority boundary is
  // discarded instead of asking the provider to rewrite unsupported chess.
  if (has_non_repairable_authority_issue(issues))
    return RepairDecision::skip_authority;

  // Keep one repair pass only for manual requests that have no deterministic
  // native fallback (for example some profile/context answers).
  return RepairDecision::attempt;
}

std::string_view repair_skip_reason(const RepairDecision decision) {
  switch (decision) {
    case RepairDecision::skip_automatic:
      return "automatic_repair_suppressed";
    case RepairDecision::skip_native_fallback:
      return "native_fallback_preferred";
    case RepairDecision::skip_authority:
      return "validation_nonrepairable";
    case RepairDecision::skip_empty:
      return "validation_empty";
    case RepairDecision::attempt:
      break;
  }
  return {};
}

LLMInteractionContract provider_interaction_contract(
    const interaction::ResolvedInteractionPlan& plan) {
  LLMInteractionContract contract;
  contract.request_kind = std::string(interaction::to_string(plan.request_kind));
  contract.answer_intent = std::string(interaction::to_string(plan.answer_intent));
  contract.elo = plan.elo;
  if (plan.color) contract.color = std::string(interaction::to_string(*plan.color));
  if (plan.game_mode) {
    contract.game_mode = std::string(interaction::to_string(*plan.game_mode));
  }
  if (plan.time_control) {
    contract.time_control = std::string(interaction::to_string(*plan.time_control));
  }
  for (const auto action : plan.actions) {
    const auto& catalog = interaction::action_catalog();
    const auto it = std::find_if(
        catalog.begin(), catalog.end(),
        [action](const auto& descriptor) { return descriptor.action == action; });
    if (it != catalog.end() && it->mutates_session) {
      contract.requested_actions.emplace_back(it->semantic_id);
    }
  }
  return contract;
}

void apply_native_client_actions(
    const interaction::ResolvedInteractionPlan& interaction_plan,
    const interaction::ActionChain& interaction_chain,
    CoachResponse& response) {
  const auto requested_color = interaction_plan.color.has_value()
      ? std::optional<std::string>{std::string(interaction::to_string(*interaction_plan.color))}
      : std::nullopt;
  const auto add_client_action =
      [&](std::string id, std::optional<int> elo = std::nullopt,
          std::optional<std::string> color = std::nullopt) {
        const auto duplicate = std::find_if(
            response.client_actions.begin(), response.client_actions.end(),
            [&](const CoachClientAction& action) { return action.id == id; });
        if (duplicate == response.client_actions.end()) {
          response.client_actions.push_back({std::move(id), elo, std::move(color)});
        }
      };
  if (interaction_chain.contains(interaction::PrimitiveAction::start_human_bot)) {
    add_client_action("open_bot_game", interaction_plan.elo, requested_color);
  } else if (interaction_chain.contains(
                 interaction::PrimitiveAction::start_stockfish_game)) {
    if (interaction_plan.elo.has_value() || requested_color.has_value()) {
      add_client_action("open_bot_game", interaction_plan.elo, requested_color);
    } else {
      add_client_action("open_bot_game_setup");
    }
  }
  if (interaction_chain.contains(interaction::PrimitiveAction::resume_bot_game)) {
    add_client_action("resume_bot_game");
  }
  if (interaction_chain.contains(
          interaction::PrimitiveAction::resign_active_bot_game)) {
    add_client_action("resign_active_bot_game");
  }
}

void apply_deterministic_interaction_actions(
    const interaction::ResolvedInteractionPlan& interaction_plan,
    const interaction::ActionChain& interaction_chain,
    const std::vector<EvidenceItem>& evidence, CoachResponse& response) {
  // Pure board/session actions are completed natively. They must not trigger a
  // remote provider call merely to render prose around an already-known action.
  // Client actions are transported separately before this fallback path; this
  // function only supplies native board/fallback data for language-free turns.
  std::size_t board_move_limit = 0;
  std::string_view fallback_kind = "position_explanation";

  if (interaction_chain.contains(interaction::PrimitiveAction::show_candidates) ||
      interaction_chain.contains(interaction::PrimitiveAction::show_line)) {
    board_move_limit = 2;
    fallback_kind = "candidate_comparison";
  } else if (interaction_chain.contains(interaction::PrimitiveAction::show_move) ||
             interaction_chain.contains(interaction::PrimitiveAction::play_move)) {
    board_move_limit = 1;
  } else if (interaction_plan.has_action(interaction::PrimitiveAction::analyze_position)) {
    board_move_limit = 1;
  }

  if (interaction_plan.has_action(interaction::PrimitiveAction::analyze_position)) {
    if (std::find(interaction_plan.semantic_ids.begin(),
                  interaction_plan.semantic_ids.end(), "find_worst") !=
        interaction_plan.semantic_ids.end()) {
      fallback_kind = "worst_move_analysis";
    } else if (std::find(interaction_plan.semantic_ids.begin(),
                         interaction_plan.semantic_ids.end(),
                         "find_fastest_loss") != interaction_plan.semantic_ids.end()) {
      fallback_kind = "fastest_loss_analysis";
    }
  }

  if (board_move_limit > 0) {
    response.board_moves =
        native_fallback_board_moves(evidence, fallback_kind, board_move_limit);
  }

  // A deterministic best-move turn is a successful native answer, not a
  // provider failure. Keep the chess fact native and let Flutter localize only
  // the presentation string through the existing ARB bestMoveText template.
  if (!response.board_moves.empty() &&
      std::find(interaction_plan.semantic_ids.begin(),
                interaction_plan.semantic_ids.end(), "find_best") !=
          interaction_plan.semantic_ids.end()) {
    response.native_answer_kind = "best_move";
  } else if (!response.board_moves.empty() &&
             std::find(interaction_plan.semantic_ids.begin(),
                       interaction_plan.semantic_ids.end(), "find_worst") !=
                 interaction_plan.semantic_ids.end()) {
    response.native_answer_kind = "worst_move";
  }

  response.validation_passed = true;
  response.validation_repaired = false;
  response.provider_error_code.clear();
}

void apply_native_safe_fallback(const std::string_view fallback_kind,
                                const std::vector<EvidenceItem>& evidence,
                                CoachResponse& response) {
  if (fallback_kind == "position_explanation") {
    response.board_moves = native_fallback_board_moves(evidence, fallback_kind, 1);
  } else if (fallback_kind == "candidate_comparison") {
    response.board_moves = native_fallback_board_moves(evidence, fallback_kind, 2);
  } else if (fallback_kind == "worst_move_analysis" ||
             fallback_kind == "fastest_loss_analysis") {
    // This is an intentionally harmful native analysis focus, not advice.
    response.board_moves = native_fallback_board_moves(evidence, fallback_kind, 1);
  }
}

void enforce_teaching_plan_contract(const CoachRequest& request,
                                    const TeachingPlan& teaching_plan,
                                    const StructuredCoachContent& content,
                                    ResponseValidationReport& report) {
  if (static_cast<int>(content.recommendations.size()) >
      teaching_plan.max_recommendations) {
    report.issues.push_back("teaching_recommendation_limit_exceeded");
  }
  if (static_cast<int>(content.concepts.size()) > teaching_plan.max_concepts) {
    report.issues.push_back("teaching_concept_limit_exceeded");
  }
  if (teaching_plan.reveal_level == 0 && !content.recommendations.empty()) {
    report.issues.push_back("teaching_move_revealed_by_recommendation");
  }
  if (request.mode == CoachMode::quiz && content.follow_up_question.empty()) {
    report.issues.push_back("quiz_move_question_missing");
  }
  report.valid = report.issues.empty();
}

ProviderStageResult provider_content(
    const std::shared_ptr<const LLMProvider>& provider,
    ValidatedResponseCache& response_cache,
    const CoachRequest& request, const CoachContext& context,
    const QueryPlan& plan,
    const interaction::ResolvedInteractionPlan& interaction_plan,
    const TeachingPlan& teaching_plan,
    const std::vector<EvidenceItem>& evidence,
    const std::vector<EvidenceItem>& provider_evidence_basis,
    const std::optional<ChessVerdictContract>& chess_verdict,
    const std::optional<ChessVerdictReview>& verdict_review,
    const bool has_verified_learner_feedback) {
  ProviderStageResult output;

  const auto request_build_started = std::chrono::steady_clock::now();
  ProviderInputOptimizationStats optimization_stats;
  auto provider_request = make_llm_provider_request(
      request, context, plan, teaching_plan, provider_evidence_basis,
      &optimization_stats);
  provider_request.interaction = provider_interaction_contract(interaction_plan);
  provider_request.chess_verdict = chess_verdict;
  provider_request.verdict_review = verdict_review;
  provider_request.has_verified_learner_feedback = has_verified_learner_feedback;
  output.request_build_ms = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - request_build_started)
          .count());
  output.provider_evidence_count = provider_request.evidence.size();
  output.provider_evidence_input_count = optimization_stats.input_items;
  output.provider_exact_duplicates_dropped =
      optimization_stats.exact_duplicate_items;
  output.provider_compacted_items = optimization_stats.compacted_items;
  output.provider_compaction_savings_tokens =
      optimization_stats.estimated_compaction_savings_tokens;
  output.provider_estimated_input_tokens =
      optimization_stats.estimated_input_tokens;
  output.provider_estimated_selected_tokens =
      optimization_stats.estimated_selected_tokens;
  output.provider_evidence_budget_tokens =
      optimization_stats.evidence_budget_tokens;
  output.response_cache_requested = true;
  const std::string provider_cache_identity =
      provider ? provider->cache_identity() : std::string{"unavailable"};
  const auto cache_key_started = std::chrono::steady_clock::now();
  const auto response_cache_key =
      response_cache.prepare_key(provider_cache_identity, provider_request,
                                 evidence);
  output.response_cache_key_ms = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - cache_key_started)
          .count());
  if (auto cached = response_cache.lookup(response_cache_key)) {
    output.content = std::move(*cached);
    output.response_cache_hit = true;
    return output;
  }
  if (!provider || !provider->available()) {
    output.provider_error_code = "provider_unavailable";
    output.safe_fallback_kind =
        safe_fallback_kind(request, provider_request.evidence, plan);
    output.fallback_reason = "provider_unavailable";
    if (!output.safe_fallback_kind.empty()) output.validation_passed = true;
    return output;
  }
  if (request.mode == CoachMode::quiz &&
      !has_visible_quiz_candidate(provider_request.evidence)) {
    output.provider_error_code = "quiz_candidate_evidence_unavailable";
    return output;
  }
  const auto provider_started = std::chrono::steady_clock::now();
  auto result = provider->complete(provider_request);
  output.provider_ms += static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - provider_started)
          .count());
  ++output.provider_calls;

  // A schema/parser mismatch is frequently transient provider output rather
  // than a chess-grounding failure. Retry the exact bounded request once before
  // degrading to the native safe fallback. This keeps the retry deterministic
  // and prevents malformed output from ever becoming user-visible.
  if (!result.ok() &&
      result.failure_kind == LLMProviderFailureKind::output_invalid) {
    const auto retry_started = std::chrono::steady_clock::now();
    auto retry = provider->complete(provider_request);
    output.repair_provider_ms += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - retry_started)
            .count());
    ++output.provider_calls;
    if (retry.ok()) {
      result = std::move(retry);
    } else {
      output.provider_error_code = retry.error_code.empty()
          ? result.error_code
          : retry.error_code;
      output.safe_fallback_kind =
          safe_fallback_kind(request, provider_request.evidence, plan);
      output.fallback_reason = "provider_output_invalid_after_retry";
      if (!output.safe_fallback_kind.empty()) output.validation_passed = true;
      return output;
    }
  }
  if (!result.ok()) {
    output.provider_error_code = result.error_code;
    output.safe_fallback_kind =
        safe_fallback_kind(request, provider_request.evidence, plan);
    output.fallback_reason = "provider_failed";
    if (!output.safe_fallback_kind.empty()) output.validation_passed = true;
    return output;
  }

  ResponseValidator validator;
  const auto validation_started = std::chrono::steady_clock::now();
  auto report = validator.validate(result.content, context.position_fen, evidence,
                                   &provider_request.evidence,
                                   provider_request.needs_profile, true,
                                   &provider_request.chess_verdict,
                                   &provider_request.verdict_review);
  enforce_teaching_plan_contract(request, teaching_plan, result.content, report);
  output.validation_ms += static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - validation_started)
          .count());
  if (report.valid) {
    try {
      auto rendered = VerifiedFactRenderer{}.render(result.content, evidence);
      response_cache.store(response_cache_key, rendered);
      output.content = std::move(rendered);
      return output;
    } catch (const std::invalid_argument& error) {
      output.validation_issues = {error.what()};
      output.safe_fallback_kind =
          safe_fallback_kind(request, provider_request.evidence, plan);
      output.fallback_reason = "renderer_rejected";
      output.validation_passed = !output.safe_fallback_kind.empty();
      return output;
    }
  }

  const auto fallback_kind =
      safe_fallback_kind(request, provider_request.evidence, plan);
  const auto repair = repair_decision(request, report.issues, fallback_kind);
  if (repair != RepairDecision::attempt) {
    output.validation_issues = std::move(report.issues);
    output.safe_fallback_kind = fallback_kind;
    output.fallback_reason = std::string(repair_skip_reason(repair));
    output.validation_passed = !output.safe_fallback_kind.empty();
    return output;
  }

  provider_request.repair_candidate = result.content;
  provider_request.validation_feedback = report.issues;
  const auto repair_provider_started = std::chrono::steady_clock::now();
  auto repaired = provider->complete(provider_request);  // One repair pass only.
  output.repair_provider_ms += static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - repair_provider_started)
          .count());
  ++output.provider_calls;
  if (repaired.ok()) {
    const auto repair_validation_started = std::chrono::steady_clock::now();
    auto repaired_report = validator.validate(
        repaired.content, context.position_fen, evidence,
        &provider_request.evidence, provider_request.needs_profile, true,
        &provider_request.chess_verdict, &provider_request.verdict_review);
    enforce_teaching_plan_contract(request, teaching_plan, repaired.content,
                                   repaired_report);
    output.validation_ms += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - repair_validation_started)
            .count());
    if (repaired_report.valid) {
      try {
        auto rendered = VerifiedFactRenderer{}.render(repaired.content, evidence);
        response_cache.store(response_cache_key, rendered);
        output.content = std::move(rendered);
        output.repaired = true;
        return output;
      } catch (const std::invalid_argument& error) {
        output.validation_issues = {error.what()};
        output.safe_fallback_kind =
            safe_fallback_kind(request, provider_request.evidence, plan);
        output.fallback_reason = "repair_renderer_rejected";
        output.validation_passed = !output.safe_fallback_kind.empty();
        return output;
      }
    }

    // Structured errors can also appear in the prose. After the single repair
    // pass, discarding only metadata would expose an ungrounded answer.
    output.validation_issues = std::move(repaired_report.issues);
    output.fallback_reason = "repair_validation_failed";
  } else {
    output.validation_issues = std::move(report.issues);
    output.validation_issues.push_back("repair_failed");
    output.provider_error_code = repaired.error_code;
    output.fallback_reason = "repair_provider_failed";
  }
  output.safe_fallback_kind =
      safe_fallback_kind(request, provider_request.evidence, plan);
  if (output.fallback_reason.empty()) output.fallback_reason = "validation_failed";
  output.validation_passed = !output.safe_fallback_kind.empty();
  return output;
}

void apply_provider_content(const StructuredCoachContent& content,
                            CoachResponse& response) {
  response.answer = content.answer;
  response.follow_up_question = content.follow_up_question;
  response.claims = content.claims;
  response.concepts = content.concepts;
  response.recommendations = content.recommendations;
  response.evidence_references = content.evidence_ids;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Orchestration
// -----------------------------------------------------------------------------

CoachOrchestrator::CoachOrchestrator(
    EvidenceSources evidence_sources,
    std::shared_ptr<const LLMProvider> provider,
    std::function<void(const CoachLearningAttempt&)> record_learning,
    std::function<void(const CoachPipelineTrace&)> record_diagnostics)
    : evidence_retriever_(std::move(evidence_sources)),
      provider_(std::move(provider)),
      record_learning_(std::move(record_learning)),
      record_diagnostics_(std::move(record_diagnostics)) {}

bool CoachOrchestrator::has_pending_move_question(
    const std::optional<std::string>& session_id,
    const std::optional<std::string>& profile_id,
    const std::optional<std::string>& question_fen) const {
  return sessions_.has_pending_move_question(session_id, profile_id, question_fen);
}

void CoachOrchestrator::restore_session(
    const std::string& session_id, const CoachSessionState& state) const {
  sessions_.restore(session_id, state);
}

std::optional<CoachSessionState> CoachOrchestrator::session_state(
    const std::optional<std::string>& session_id,
    const std::optional<std::string>& profile_id) const {
  return sessions_.snapshot(session_id, profile_id);
}

PositionAnalysisCacheStats CoachOrchestrator::position_cache_stats() const {
  return position_analysis_stage_.cache_stats();
}

ValidatedResponseCacheStats CoachOrchestrator::response_cache_stats() const {
  return response_cache_.stats();
}

CoachResponse CoachOrchestrator::handle(const CoachRequest& request) const {
  using Clock = std::chrono::steady_clock;
  const auto total_started = Clock::now();
  CoachPipelineTrace trace;
  trace.automatic_turn = request.automatic_turn;
  if (request.move_attribution) {
    trace.move_mover_color = request.move_attribution->mover_color.value_or("");
    trace.move_learner_color = request.move_attribution->learner_color.value_or("");
    trace.move_mover_role = request.move_attribution->mover_role;
    trace.move_played_uci = request.move_attribution->played_move_uci.value_or("");
    trace.move_verified_learner = request.move_attribution->verified_learner_move;
  }
  const auto elapsed_ms = [](const Clock::time_point started) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - started)
            .count());
  };

  auto stage_started = Clock::now();
  ResolvedCoachTurn turn = sessions_.resolve(request);
  trace.session_ms = elapsed_ms(stage_started);
  if (turn.learning_attempt && record_learning_ &&
      turn.learning_attempt->profile_id) {
    try {
      record_learning_(*turn.learning_attempt);
    } catch (...) {
      // Learning telemetry must not suppress move feedback.
    }
  }
  const CoachSessionState* session = turn.has_previous ? &turn.previous : nullptr;
  if (session != nullptr) {
    trace.learner_attempt_status = session->last_attempt_status;
    trace.learner_move_classification = session->last_attempt_classification;
  }

  const auto verdict_challenge = resolve_verdict_challenge(turn.request, session);
  if (verdict_challenge.has_value()) {
    // Persist only the native review binding on the durable turn. The user-visible
    // current board remains unchanged; routed_request below may temporarily point
    // analysis at the original root board.
    turn.request.verdict_challenge = verdict_challenge;
  }
  CoachRequest routed_request = turn.request;
  if (verdict_challenge && verdict_challenge->analysis_fen) {
    routed_request.position_fen = verdict_challenge->analysis_fen;
  }

  stage_started = Clock::now();
  const DomainRoute route = ChessDomainRouter{}.route(
      routed_request, session);
  trace.route_ms = elapsed_ms(stage_started);
  trace.routed_intent = diagnostic_intent_name(route.intent);
  trace.context_intent = diagnostic_intent_name(route.context_intent);
  trace.explicit_current_intent = route.explicit_current_intent;
  if (!route.chess_domain || route.intent == CoachIntent::off_topic) {
    // The chess-only route is authoritative. Avoid profile retrieval, position
    // extraction and cache-key construction for a rejected request.
    CoachResponse response;
    response.intent = route.intent;
    response.answer_type = answer_type_for_mode(turn.request.mode);
    response.accepted = false;
    trace.accepted = false;
    trace.total_ms = elapsed_ms(total_started);
    if (record_diagnostics_) {
      try {
        record_diagnostics_(trace);
      } catch (...) {
        // Read-only diagnostics must never affect a Coach answer.
      }
    }
    return response;
  }
  const CoachIntent topic =
      route.intent == CoachIntent::follow_up &&
              route.context_intent != CoachIntent::unknown
          ? route.context_intent
          : route.intent;

  // Explicit current-board analysis questions override a persisted quiz/hint
  // continuation. Session history remains available, but a stale exercise may
  // never intercept a direct request such as "best move" or "worst move".
  const auto interaction_state = interaction_state_for_request(routed_request);
  const auto interaction_selection =
      interaction::PrimaryInteractionPlanner{}.classify(
          turn.request.user_text, interaction_state);
  const bool explicit_direct_analysis =
      interaction_selection.contains("find_best") ||
      interaction_selection.contains("find_worst") ||
      interaction_selection.contains("find_fastest_loss") ||
      interaction_selection.contains("evaluate_move") ||
      interaction_selection.contains("evaluate_position") ||
      interaction_selection.contains("find_blunder") ||
      interaction_selection.contains("find_mistake") ||
      verdict_challenge.has_value();

  // Keep one scored trainer question authoritative until the learner actually
  // plays a move. Reissuing the Quiz action on the same board must not spend a
  // second provider call or append the same question again. Hint/explain modes
  // remain available and can advance the lesson without replacing the pending
  // scoring state.
  if (turn.request.mode == CoachMode::quiz && session != nullptr &&
      !explicit_direct_analysis &&
      session->has_open_scored_question_for(turn.request)) {
    CoachResponse response;
    response.intent = route.intent;
    response.answer_type = CoachAnswerType::quiz;
    response.accepted = true;
    response.validation_passed = true;
    response.safe_fallback_kind = "quiz_question_pending";
    trace.accepted = true;
    trace.validation_passed = true;
    trace.safe_fallback_kind = response.safe_fallback_kind;
    trace.teaching_objective = "await_existing_quiz_attempt";
    trace.teaching_delivery_mode = "awaiting_attempt";
    sessions_.remember(turn, topic, response);
    trace.total_ms = elapsed_ms(total_started);
    if (record_diagnostics_) {
      try {
        record_diagnostics_(trace);
      } catch (...) {
        // Read-only diagnostics must never affect a Coach answer.
      }
    }
    return response;
  }

  CoachRequest effective_request = routed_request;

  // Primary semantic/action pipeline. The legacy QueryPlanner below is retained
  // only as an evidence-budget adapter while retrieval is migrated. It no longer
  // owns high-confidence coach intent such as best/worst/fastest-loss/tactics.
  const auto interaction_parameters =
      interaction::parse_interaction_parameters(effective_request.user_text);
  const auto interaction_plan =
      interaction::InteractionRequirementResolver{}.resolve(
          interaction_selection, interaction_parameters, interaction_state);
  const auto interaction_chain =
      interaction::ActionChainEngine{}.build(interaction_plan, interaction_state);
  const auto interaction_answer_gate =
      interaction::AnswerGate{}.decide(interaction_plan, interaction_chain);
  trace.interaction_request_kind =
      std::string(interaction::to_string(interaction_plan.request_kind));
  trace.interaction_answer_intent =
      std::string(interaction::to_string(interaction_plan.answer_intent));
  trace.answer_llm_used = interaction_answer_gate.answer_llm_required;

  stage_started = Clock::now();
  QueryPlan plan = QueryPlanner{}.plan(effective_request, route);
  apply_primary_interaction_constraints(interaction_plan, plan);

  trace.planning_ms = elapsed_ms(stage_started);
  trace.analysis_mode = diagnostic_analysis_mode_name(plan.analysis_mode);
  trace.analysis_mode_explicit = plan.analysis_mode_explicit;
  trace.evidence_plan_confidence = plan.evidence_plan.confidence;
  trace.evidence_plan_source_count = plan.evidence_plan.sources.size();
  trace.evidence_plan_need_count = plan.evidence_plan.needs.size();
  trace.evidence_plan_freshness = std::string(
      diagnostic_evidence_freshness_name(plan.evidence_plan.freshness));
  trace.evidence_plan_interaction = std::string(
      diagnostic_evidence_interaction_name(plan.evidence_plan.interaction));
  trace.evidence_plan_elo_target = std::string(
      diagnostic_evidence_elo_target_name(plan.evidence_plan.elo_target));

  stage_started = Clock::now();
  const CoachContext context =
      ContextBuilder{}.build(effective_request, plan, session);
  trace.context_ms = elapsed_ms(stage_started);

  stage_started = Clock::now();
  auto retrieved = evidence_retriever_.retrieve(
      effective_request, plan, context);
  trace.retrieval_ms = elapsed_ms(stage_started);
  trace.retrieved_evidence_count = retrieved.items.size();
  auto evidence = std::move(retrieved.items);

  stage_started = Clock::now();
  const auto position_analysis =
      position_analysis_stage_.analyze(effective_request, plan, evidence);
  trace.position_analysis_ms = elapsed_ms(stage_started);
  trace.position_cache_requested = position_analysis.cache_requested;
  trace.position_cache_any_hit = position_analysis.cache_any_hit;
  trace.position_cache_full_hit = position_analysis.cache_full_hit;

  stage_started = Clock::now();
  append_practicality(plan, retrieved.candidate_moves,
                      retrieved.practicality_player,
                      position_analysis.artifacts, evidence);
  append_human_model_evidence(plan, retrieved, evidence);
  const bool current_turn_is_completed_move =
      effective_request.user_move_uci.has_value() ||
      effective_request.user_move_success_confirmed ||
      effective_request.user_move_error_confirmed;
  if (session != nullptr && !session->last_attempt_status.empty() &&
      current_turn_is_completed_move) {
    const auto completed_analysis =
        evidence_retriever_.completed_move_analysis(
            effective_request, session->question_board);
    if (auto contrast = completed_move_contrast(
            effective_request, session,
            position_analysis.artifacts.features
                ? &*position_analysis.artifacts.features : nullptr,
            completed_analysis ? &*completed_analysis : nullptr))
      evidence.push_back(std::move(*contrast));
  }
  trace.practicality_ms = elapsed_ms(stage_started);
  trace.final_evidence_count = evidence.size();
  const auto native_summary = summarize_native_candidate_evidence(evidence);
  trace.native_candidate_count = native_summary.candidates;
  trace.native_fact_count = native_summary.facts;
  trace.native_focus_kind = native_summary.focus_kind;

  auto chess_verdict = build_chess_verdict(
      effective_request, plan, retrieved.candidate_moves);
  const auto verdict_review = review_chess_verdict(
      verdict_challenge, chess_verdict);
  if (verdict_review &&
      verdict_review->outcome == VerdictReviewOutcome::inconclusive &&
      verdict_challenge && verdict_challenge->previous_verdict.authoritative) {
    // A failed/incomplete recheck is not evidence that the user's counterclaim
    // is correct. Keep the last authoritative judgment until native evidence
    // actually proves a change.
    chess_verdict = verdict_challenge->previous_verdict;
  }
  if (verdict_review.has_value()) {
    trace.verdict_review_outcome = std::string(
        verdict_review_outcome_name(verdict_review->outcome));
    trace.verdict_review_move =
        verdict_review->challenged_move_uci.value_or("");
  }
  if (chess_verdict.has_value()) {
    trace.verdict_authoritative = chess_verdict->authoritative;
    trace.verdict_position = std::string(
        position_verdict_name(chess_verdict->position_verdict));
    trace.verdict_move = std::string(
        move_verdict_name(chess_verdict->move_verdict));
    trace.verdict_evaluated_move =
        chess_verdict->evaluated_move_uci.value_or("");
    trace.verdict_basis = chess_verdict->basis;
  }

  const auto verdict_requirement =
      verdict_grounding_requirement(interaction_plan, verdict_challenge);
  const bool verdict_required =
      verdict_requirement != VerdictGroundingRequirement::none;
  const bool verdict_available =
      verdict_grounding_available(verdict_requirement, chess_verdict);
  const bool verdict_grounding_blocked = verdict_required && !verdict_available;
  trace.verdict_grounding_required = verdict_required;
  trace.verdict_grounding_available = verdict_available;
  trace.verdict_grounding_blocked = verdict_grounding_blocked;

  const auto provider_basis = aggregate_provider_evidence(plan, evidence);
  trace.aggregated_evidence_count = provider_basis.items.size();
  trace.evidence_needs_satisfied =
      provider_basis.aggregation.satisfied_needs.size();
  trace.evidence_needs_missing = provider_basis.aggregation.missing_needs.size();
  trace.evidence_duplicates_removed =
      provider_basis.aggregation.duplicates_removed;
  trace.evidence_conflicts_resolved =
      provider_basis.aggregation.conflicts_resolved;

  stage_started = Clock::now();
  TeachingPlan teaching_plan =
      TeachingPlanner{}.plan(effective_request, topic, plan, evidence, session);
  if ((interaction_plan.request_kind == interaction::InteractionRequestKind::app_action ||
       interaction_plan.request_kind ==
           interaction::InteractionRequestKind::conversation_control) &&
      interaction_answer_gate.answer_llm_required) {
    teaching_plan.objective_id =
        interaction_plan.answer_intent == interaction::InteractionAnswerIntent::coach
            ? "acknowledge_live_coaching"
            : "acknowledge_app_action";
    teaching_plan.delivery_mode = "action_acknowledgement";
    teaching_plan.reveal_level = 0;
    teaching_plan.hint_level = 0;
    teaching_plan.ask_question = false;
    teaching_plan.max_recommendations = 0;
    teaching_plan.max_concepts = 0;
    teaching_plan.skill_id = "interaction.app_action";
    teaching_plan.skill_family = "interaction";
    teaching_plan.target.clear();
  }
  trace.teaching_plan_ms = elapsed_ms(stage_started);
  trace.teaching_objective = teaching_plan.objective_id;
  trace.teaching_delivery_mode = teaching_plan.delivery_mode;
  trace.teaching_skill_id = teaching_plan.skill_id;

  CoachResponse response;
  response.chess_verdict = chess_verdict;
  response.intent = route.intent;
  response.answer_type = answer_type_for_mode(turn.request.mode);
  response.accepted = route.chess_domain && route.intent != CoachIntent::off_topic;
  trace.accepted = response.accepted;
  if (response.accepted) {
    // Product/session actions are native authority and must survive mixed turns
    // that also require an LLM language response.
    apply_native_client_actions(interaction_plan, interaction_chain, response);
  }
  if (response.accepted && verdict_grounding_blocked) {
    // Evaluative chess questions may never degrade into an ungrounded language
    // opinion. If native analysis cannot establish the required position/move
    // verdict, return a localized safe block and do not call the provider.
    response.validation_passed = true;
    response.safe_fallback_kind = "verdict_grounding_unavailable";
    trace.safe_fallback_kind = response.safe_fallback_kind;
    trace.fallback_reason = "native_verdict_grounding_unavailable";
    trace.answer_llm_used = false;
  } else if (response.accepted && interaction_answer_gate.answer_llm_required) {
    const auto provider_stage =
        provider_content(provider_, response_cache_, effective_request, context, plan,
                         interaction_plan, teaching_plan, evidence, provider_basis.items,
                         chess_verdict, verdict_review,
                         session != nullptr && !session->last_attempt_status.empty() &&
                             current_turn_is_completed_move);
    trace.provider_request_ms = provider_stage.request_build_ms;
    trace.response_cache_key_ms = provider_stage.response_cache_key_ms;
    trace.provider_ms = provider_stage.provider_ms;
    trace.validation_ms = provider_stage.validation_ms;
    trace.repair_provider_ms = provider_stage.repair_provider_ms;
    trace.provider_evidence_count = provider_stage.provider_evidence_count;
    trace.provider_evidence_input_count =
        provider_stage.provider_evidence_input_count;
    trace.provider_exact_duplicates_dropped =
        provider_stage.provider_exact_duplicates_dropped;
    trace.provider_compacted_items = provider_stage.provider_compacted_items;
    trace.provider_compaction_savings_tokens =
        provider_stage.provider_compaction_savings_tokens;
    trace.provider_estimated_input_tokens =
        provider_stage.provider_estimated_input_tokens;
    trace.provider_estimated_selected_tokens =
        provider_stage.provider_estimated_selected_tokens;
    trace.provider_evidence_budget_tokens =
        provider_stage.provider_evidence_budget_tokens;
    trace.response_cache_requested = provider_stage.response_cache_requested;
    trace.response_cache_hit = provider_stage.response_cache_hit;
    trace.provider_calls = provider_stage.provider_calls;
    trace.provider_error_code = provider_stage.provider_error_code;
    trace.validation_issues = provider_stage.validation_issues;
    trace.safe_fallback_kind = provider_stage.safe_fallback_kind;
    trace.fallback_reason = provider_stage.fallback_reason;
    response.validation_passed = provider_stage.validation_passed;
    response.validation_repaired = provider_stage.repaired;
    response.validation_issues = provider_stage.validation_issues;
    response.provider_error_code = provider_stage.provider_error_code;
    response.safe_fallback_kind = provider_stage.safe_fallback_kind;
    if (provider_stage.content.has_value()) {
      apply_provider_content(*provider_stage.content, response);
    } else if (!provider_stage.safe_fallback_kind.empty()) {
      apply_native_safe_fallback(provider_stage.safe_fallback_kind, evidence,
                                 response);
    }
  } else if (response.accepted) {
    apply_deterministic_interaction_actions(interaction_plan, interaction_chain,
                                            evidence, response);
  }
  const auto action_fulfillment =
      interaction::ActionFulfillmentValidator{}.validate(
          interaction_plan, interaction_chain, response);
  response.action_fulfillment_required = action_fulfillment.required;
  response.action_fulfillment_passed = action_fulfillment.passed;
  response.action_fulfillment_issues = action_fulfillment.issues;
  trace.action_fulfillment_required = action_fulfillment.required;
  trace.action_fulfillment_passed = action_fulfillment.passed;
  trace.requested_actions = action_fulfillment.requested_actions;
  trace.transported_client_actions = action_fulfillment.transported_actions;
  trace.action_fulfillment_issues = action_fulfillment.issues;
  trace.validation_passed = response.validation_passed;
  trace.validation_repaired = response.validation_repaired;
  trace.native_answer_kind = response.native_answer_kind;
  trace.response_renderable =
      !response.answer.empty() || !response.follow_up_question.empty() ||
      !response.client_actions.empty() ||
      (!response.native_answer_kind.empty() && !response.board_moves.empty());
  response.evidence = std::move(evidence);

  ResolvedCoachTurn remembered_turn = turn;
  remembered_turn.request.verdict_challenge = verdict_challenge;
  sessions_.remember(remembered_turn, topic, response, &plan,
      retrieved.candidate_moves ? &*retrieved.candidate_moves : nullptr,
      &teaching_plan);
  trace.total_ms = elapsed_ms(total_started);
  if (record_diagnostics_) {
    try {
      record_diagnostics_(trace);
    } catch (...) {
      // Read-only diagnostics must never affect a Coach answer.
    }
  }
  return response;
}

}  // namespace kchess::ai
