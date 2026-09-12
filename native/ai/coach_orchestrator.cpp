#include "coach_orchestrator.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "context_builder.h"
#include "domain_router.h"
#include "dto/evidence.h"
#include "dto/query_plan.h"
#include "evidence_retriever.h"
#include "position/exploitation_planner.h"
#include "position/plan_generator.h"
#include "position/position_features.h"
#include "position/tactical_detector.h"
#include "position/weakness_analyzer.h"
#include "practicality/player_practicality.h"
#include "practicality/practicality_engine.h"
#include "providers/llm_provider.h"
#include "query_planner.h"
#include "validation/response_validator.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Position-analysis stage
// -----------------------------------------------------------------------------

struct AnalysisArtifacts {
  std::optional<PositionFeatures> features;
  std::optional<TacticalAnalysis> tactics;
};

AnalysisArtifacts analyze_stub(
    const CoachRequest& request, const QueryPlan& plan,
    const std::optional<PositionFeatures>& retrieved_features,
    std::vector<EvidenceItem>& evidence) {
  AnalysisArtifacts artifacts;
  const auto wants = [&plan](EvidenceKind kind) {
    return std::find(plan.evidence.begin(), plan.evidence.end(), kind) !=
           plan.evidence.end();
  };
  const bool wants_features = wants(EvidenceKind::position_features);
  const bool wants_weaknesses = wants(EvidenceKind::position_weaknesses);
  const bool wants_exploitation = wants(EvidenceKind::weakness_exploitation);
  const bool wants_plans = wants(EvidenceKind::strategic_plans);
  const bool wants_tactics = wants(EvidenceKind::tactical_motifs);
  const bool wants_practicality = wants(EvidenceKind::practicality);
  if ((!wants_features && !wants_weaknesses && !wants_exploitation &&
       !wants_plans && !wants_tactics && !wants_practicality) ||
      !request.position_fen.has_value()) {
    return artifacts;
  }

  artifacts.features = retrieved_features.has_value()
                           ? retrieved_features
                           : std::optional<PositionFeatures>(
                                 PositionFeatureExtractor{}.extract(
                                     *request.position_fen));
  const auto& features = *artifacts.features;

  if (wants_weaknesses || wants_exploitation || wants_plans) {
    WeaknessAnalyzer weakness_analyzer;
    const auto weaknesses = weakness_analyzer.analyze(features);
    if (wants_weaknesses) {
      evidence.push_back(weakness_analyzer.evidence(weaknesses));
    }

    ExploitationPlanner exploitation_planner;
    const auto exploitation = exploitation_planner.plan(weaknesses);
    if (wants_exploitation) {
      evidence.push_back(exploitation_planner.evidence(exploitation));
    }
    if (wants_plans) {
      PlanGenerator plan_generator;
      evidence.push_back(plan_generator.evidence(
          plan_generator.generate(features, weaknesses, exploitation)));
    }
  }

  if (wants_tactics) {
    TacticalDetector tactical_detector;
    artifacts.tactics = tactical_detector.analyze(*request.position_fen);
    evidence.push_back(tactical_detector.evidence(*artifacts.tactics));
  }
  return artifacts;
}

// -----------------------------------------------------------------------------
// Section: Practicality stage
// -----------------------------------------------------------------------------

void append_practicality(
    const QueryPlan& plan,
    const std::optional<CandidateMoveSet>& candidates,
    const std::optional<PracticalityPlayerContext>& player,
    const AnalysisArtifacts& artifacts,
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
  bool validation_passed{true};
  bool repaired{false};
};

ProviderStageResult provider_content(
    const std::shared_ptr<const LLMProvider>& provider,
    const CoachRequest& request, const CoachContext& context,
    const QueryPlan& plan, const std::vector<EvidenceItem>& evidence) {
  ProviderStageResult output;
  if (!provider || !provider->available()) return output;

  auto provider_request =
      make_llm_provider_request(request, context, plan, evidence);
  auto result = provider->complete(provider_request);
  if (!result.ok()) return output;

  ResponseValidator validator;
  auto report = validator.validate(result.content, request.position_fen, evidence);
  if (report.valid) {
    output.content = std::move(result.content);
    return output;
  }

  provider_request.repair_candidate = result.content;
  provider_request.validation_feedback = report.issues;
  auto repaired = provider->complete(provider_request);  // One repair pass only.
  if (repaired.ok()) {
    auto repaired_report =
        validator.validate(repaired.content, request.position_fen, evidence);
    if (repaired_report.valid) {
      output.content = std::move(repaired.content);
      output.repaired = true;
      return output;
    }

    // The repair pass already received the deterministic validation feedback.
    // If it still emits bad structured board metadata, strip only that metadata
    // instead of throwing away an otherwise useful natural-language answer.
    // This also prevents invalid arrows/highlights from reaching Flutter.
    auto safe_content = std::move(repaired.content);
    auto sanitized = validator.sanitize_metadata(
        safe_content, request.position_fen, evidence);
    if (!safe_content.answer.empty()) {
      output.content = std::move(safe_content);
      output.validation_issues = std::move(sanitized.issues);
      output.validation_passed = true;
      output.repaired = true;
      return output;
    }
    output.validation_issues = std::move(repaired_report.issues);
  } else {
    output.validation_issues = std::move(report.issues);
    output.validation_issues.push_back("repair_failed");
  }
  output.validation_passed = false;
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
    SmallModelSuite small_models)
    : evidence_retriever_(std::move(evidence_sources)),
      provider_(std::move(provider)),
      small_models_(std::move(small_models)) {}

CoachResponse CoachOrchestrator::handle(const CoachRequest& request) const {
  const ResolvedCoachTurn turn = sessions_.resolve(request);
  const CoachSessionState* session = turn.has_previous ? &turn.previous : nullptr;
  const DomainRoute route = ChessDomainRouter{}.route(
      turn.request, session, small_models_.intent.get());
  const QueryPlan plan = QueryPlanner{}.plan(
      turn.request, route, small_models_.context_planner.get());
  const CoachContext context = ContextBuilder{}.build(turn.request, plan, session);
  auto retrieved = evidence_retriever_.retrieve(
      turn.request, plan, context, small_models_.embeddings.get());
  auto evidence = std::move(retrieved.items);

  const auto analysis =
      analyze_stub(turn.request, plan, retrieved.position_features, evidence);
  append_practicality(plan, retrieved.candidate_moves,
                      retrieved.practicality_player, analysis, evidence);

  CoachResponse response;
  response.intent = route.intent;
  response.answer_type = answer_type_for_mode(turn.request.mode);
  response.accepted = route.chess_domain && route.intent != CoachIntent::off_topic;
  if (response.accepted) {
    const auto provider_stage =
        provider_content(provider_, turn.request, context, plan, evidence);
    response.validation_passed = provider_stage.validation_passed;
    response.validation_repaired = provider_stage.repaired;
    response.validation_issues = provider_stage.validation_issues;
    if (provider_stage.content.has_value()) {
      apply_provider_content(*provider_stage.content, response);
    }
  }
  response.evidence = std::move(evidence);

  const CoachIntent topic =
      route.intent == CoachIntent::follow_up &&
              route.context_intent != CoachIntent::unknown
          ? route.context_intent
          : route.intent;
  sessions_.remember(turn, topic, response);
  return response;
}

}  // namespace kchess::ai
