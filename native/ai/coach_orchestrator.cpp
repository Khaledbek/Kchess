#include "coach_orchestrator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "context_builder.h"
#include "domain_router.h"
#include "dto/evidence.h"
#include "dto/query_plan.h"
#include "evidence_retriever.h"
#include "position/move_contrast.h"
#include "position/position_analysis_stage.h"
#include "practicality/player_practicality.h"
#include "optimization/provider_input_optimizer.h"
#include "optimization/validated_response_cache.h"
#include "practicality/practicality_engine.h"
#include "providers/llm_provider.h"
#include "query_planner.h"
#include "teaching/teaching_planner.h"
#include "validation/response_validator.h"

namespace kchess::ai {
namespace {

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
  std::size_t provider_estimated_input_tokens{0};
  std::size_t provider_estimated_selected_tokens{0};
  std::size_t provider_evidence_budget_tokens{0};
  int provider_calls{0};
};

bool has_visible_quiz_candidate(const std::vector<EvidenceItem>& evidence) {
  return std::any_of(evidence.begin(), evidence.end(), [](const auto& item) {
    if (item.kind != EvidenceKind::candidate_moves) return false;
    const auto payload = nlohmann::json::parse(item.payload, nullptr, false);
    return payload.is_object() && payload.contains("best") &&
        payload["best"].is_object() &&
        payload["best"].contains("move_uci") &&
        payload["best"]["move_uci"].is_string() &&
        !payload["best"]["move_uci"].get<std::string>().empty();
  });
}

std::string safe_fallback_kind(const CoachRequest& request,
                               const std::vector<EvidenceItem>& visible_evidence) {
  if (request.mode == CoachMode::quiz &&
      has_visible_quiz_candidate(visible_evidence)) {
    return "quiz_question";
  }
  if (request.mode == CoachMode::hint && request.position_fen.has_value()) {
    return "hint_prompt";
  }
  return {};
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
    const QueryPlan& plan, const TeachingPlan& teaching_plan,
    const std::vector<EvidenceItem>& evidence,
    const bool has_verified_learner_feedback) {
  ProviderStageResult output;

  const auto request_build_started = std::chrono::steady_clock::now();
  ProviderInputOptimizationStats optimization_stats;
  auto provider_request = make_llm_provider_request(
      request, context, plan, teaching_plan, evidence, &optimization_stats);
  provider_request.has_verified_learner_feedback = has_verified_learner_feedback;
  output.request_build_ms = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - request_build_started)
          .count());
  output.provider_evidence_count = provider_request.evidence.size();
  output.provider_evidence_input_count = optimization_stats.input_items;
  output.provider_exact_duplicates_dropped =
      optimization_stats.exact_duplicate_items;
  output.provider_estimated_input_tokens =
      optimization_stats.estimated_input_tokens;
  output.provider_estimated_selected_tokens =
      optimization_stats.estimated_selected_tokens;
  output.provider_evidence_budget_tokens =
      optimization_stats.evidence_budget_tokens;
  output.response_cache_requested = true;
  const std::string_view provider_id =
      provider ? provider->id() : std::string_view{"unavailable"};
  const auto cache_key_started = std::chrono::steady_clock::now();
  const auto response_cache_key =
      response_cache.prepare_key(provider_id, provider_request, evidence);
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
  if (!result.ok()) {
    output.provider_error_code = result.error_code;
    if (result.output_invalid()) {
      output.safe_fallback_kind =
          safe_fallback_kind(request, provider_request.evidence);
      if (!output.safe_fallback_kind.empty()) output.validation_passed = true;
    }
    return output;
  }

  ResponseValidator validator;
  const auto validation_started = std::chrono::steady_clock::now();
  auto report = validator.validate(result.content, context.position_fen, evidence,
                                   &provider_request.evidence,
                                   provider_request.needs_profile, true,
                                   plan.needs_position);
  enforce_teaching_plan_contract(request, teaching_plan, result.content, report);
  output.validation_ms += static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - validation_started)
          .count());
  if (report.valid) {
    response_cache.store(response_cache_key, result.content);
    output.content = std::move(result.content);
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
        plan.needs_position);
    enforce_teaching_plan_contract(request, teaching_plan, repaired.content,
                                   repaired_report);
    output.validation_ms += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - repair_validation_started)
            .count());
    if (repaired_report.valid) {
      response_cache.store(response_cache_key, repaired.content);
      output.content = std::move(repaired.content);
      output.repaired = true;
      return output;
    }

    // Structured errors can also appear in the prose. After the single repair
    // pass, discarding only metadata would expose an ungrounded answer.
    output.validation_issues = std::move(repaired_report.issues);
  } else {
    output.validation_issues = std::move(report.issues);
    output.validation_issues.push_back("repair_failed");
    output.provider_error_code = repaired.error_code;
  }
  output.safe_fallback_kind =
      safe_fallback_kind(request, provider_request.evidence);
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
    SmallModelSuite small_models,
    std::function<void(const CoachLearningAttempt&)> record_learning,
    std::function<void(const CoachPipelineTrace&)> record_diagnostics)
    : evidence_retriever_(std::move(evidence_sources)),
      provider_(std::move(provider)),
      small_models_(std::move(small_models)),
      record_learning_(std::move(record_learning)),
      record_diagnostics_(std::move(record_diagnostics)) {}

bool CoachOrchestrator::has_pending_move_question(
    const std::optional<std::string>& session_id,
    const std::optional<std::string>& profile_id,
    const std::optional<std::string>& question_fen) const {
  return sessions_.has_pending_move_question(session_id, profile_id, question_fen);
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
  const auto elapsed_ms = [](const Clock::time_point started) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - started)
            .count());
  };

  auto stage_started = Clock::now();
  const ResolvedCoachTurn turn = sessions_.resolve(request);
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

  stage_started = Clock::now();
  const DomainRoute route = ChessDomainRouter{}.route(
      turn.request, session, small_models_.intent.get());
  trace.route_ms = elapsed_ms(stage_started);
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
  CoachRequest effective_request = turn.request;

  stage_started = Clock::now();
  const QueryPlan plan = QueryPlanner{}.plan(
      effective_request, route, small_models_.context_planner.get());
  trace.planning_ms = elapsed_ms(stage_started);

  stage_started = Clock::now();
  const CoachContext context =
      ContextBuilder{}.build(effective_request, plan, session);
  trace.context_ms = elapsed_ms(stage_started);

  stage_started = Clock::now();
  auto retrieved = evidence_retriever_.retrieve(
      effective_request, plan, context, small_models_.embeddings.get());
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
  if (session != nullptr && !session->last_attempt_status.empty()) {
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

  stage_started = Clock::now();
  const TeachingPlan teaching_plan =
      TeachingPlanner{}.plan(effective_request, topic, plan, evidence, session);
  trace.teaching_plan_ms = elapsed_ms(stage_started);
  trace.teaching_objective = teaching_plan.objective_id;
  trace.teaching_delivery_mode = teaching_plan.delivery_mode;
  trace.teaching_skill_id = teaching_plan.skill_id;

  CoachResponse response;
  response.intent = route.intent;
  response.answer_type = answer_type_for_mode(turn.request.mode);
  response.accepted = route.chess_domain && route.intent != CoachIntent::off_topic;
  trace.accepted = response.accepted;
  if (response.accepted) {
    const auto provider_stage =
        provider_content(provider_, response_cache_, effective_request, context, plan,
                         teaching_plan, evidence,
                         session != nullptr && !session->last_attempt_status.empty());
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
    response.validation_passed = provider_stage.validation_passed;
    response.validation_repaired = provider_stage.repaired;
    response.validation_issues = provider_stage.validation_issues;
    response.provider_error_code = provider_stage.provider_error_code;
    response.safe_fallback_kind = provider_stage.safe_fallback_kind;
    if (provider_stage.content.has_value()) {
      apply_provider_content(*provider_stage.content, response);
    }
  }
  trace.validation_passed = response.validation_passed;
  trace.validation_repaired = response.validation_repaired;
  response.evidence = std::move(evidence);

  sessions_.remember(turn, topic, response, &plan,
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
