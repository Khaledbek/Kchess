#include "evidence_retriever.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "concepts/concept_retriever.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Retrieval policy
// -----------------------------------------------------------------------------

bool requested(const QueryPlan& plan, EvidenceKind kind) {
  return std::find(plan.evidence.begin(), plan.evidence.end(), kind) !=
         plan.evidence.end();
}

std::string_view stable_id(EvidenceKind kind) {
  switch (kind) {
    case EvidenceKind::cache:
      return "cache.lookup.v1";
    case EvidenceKind::existing_analysis:
      return "analysis.existing.v1";
    case EvidenceKind::theory:
      return "theory.lookup.v1";
    case EvidenceKind::opening:
      return "opening.lookup.v1";
    case EvidenceKind::user_profile:
      return "profile.snapshot.v1";
    case EvidenceKind::engine:
      return "engine.analysis.v1";
    case EvidenceKind::conversation:
      return "conversation.session.v1";
    default:
      return {};
  }
}

bool append_source(std::vector<EvidenceItem>& items,
                   const EvidenceSource& source,
                   EvidenceKind kind,
                   const CoachRequest& request,
                   const QueryPlan& plan) {
  if (!source || !requested(plan, kind)) return false;
  auto item = source(request, plan);
  if (!item.has_value() || item->payload.empty()) return false;
  item->kind = kind;
  item->id = std::string(stable_id(kind));
  item->confidence = std::clamp(item->confidence, 0.0, 1.0);
  items.push_back(std::move(*item));
  return true;
}

void append_conversation(std::vector<EvidenceItem>& items,
                         const QueryPlan& plan,
                         const CoachContext& context) {
  if (!requested(plan, EvidenceKind::conversation) ||
      context.session_summary.empty()) {
    return;
  }
  items.push_back({
      .id = std::string(stable_id(EvidenceKind::conversation)),
      .kind = EvidenceKind::conversation,
      .payload = context.session_summary,
      .confidence = 1.0,
  });
}

void append_concepts(std::vector<EvidenceItem>& items,
                     const CoachRequest& request,
                     const QueryPlan& plan,
                     const EmbeddingModel* embeddings) {
  if (!requested(plan, EvidenceKind::chess_concepts)) return;
  auto item = ConceptRetriever{}.evidence(request.user_text, plan.concept_limit, embeddings);
  if (item.confidence > 0.0) items.push_back(std::move(item));
}

bool append_candidates(RetrievedEvidence& result,
                       const CandidateMoveSnapshot& snapshot,
                       const CoachRequest& request) {
  CandidateMoveSystem candidates;
  auto set = candidates.build(snapshot, request.user_move_uci);
  if (!set.best.has_value()) return false;
  result.items.push_back(candidates.evidence(set));
  result.candidate_moves = std::move(set);
  return true;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Evidence retrieval
// -----------------------------------------------------------------------------

EvidenceRetriever::EvidenceRetriever(EvidenceSources sources)
    : sources_(std::move(sources)) {}

RetrievedEvidence EvidenceRetriever::retrieve(
    const CoachRequest& request,
    const QueryPlan& plan,
    const CoachContext& context,
    const EmbeddingModel* embeddings) const {
  RetrievedEvidence result;

  // Cache and persisted analysis always get the first opportunity to ground a
  // request before a fresh engine search is considered.
  append_source(result.items, sources_.cache, EvidenceKind::cache, request,
                plan);
  append_source(result.items, sources_.existing_analysis,
                EvidenceKind::existing_analysis, request, plan);
  if (requested(plan, EvidenceKind::candidate_moves) &&
      sources_.existing_candidates) {
    if (auto snapshot = sources_.existing_candidates(request, plan)) {
      append_candidates(result, *snapshot, request);
    }
  }

  append_conversation(result.items, plan, context);
  append_concepts(result.items, request, plan, embeddings);

  if (requested(plan, EvidenceKind::position_features) &&
      request.position_fen.has_value()) {
    PositionFeatureExtractor extractor;
    result.position_features = extractor.extract(*request.position_fen);
    result.items.push_back(extractor.evidence(*result.position_features));
  }

  append_source(result.items, sources_.theory, EvidenceKind::theory, request,
                plan);
  append_source(result.items, sources_.opening, EvidenceKind::opening, request,
                plan);
  append_source(result.items, sources_.user_profile,
                EvidenceKind::user_profile, request, plan);
  if (requested(plan, EvidenceKind::user_profile) &&
      sources_.practicality_player) {
    result.practicality_player = sources_.practicality_player(request, plan);
  }

  result.engine_budget = EngineBudgetSystem{}.decide(request, plan, result.items);
  if (result.engine_budget.should_query()) {
    QueryPlan engine_plan = plan;
    engine_plan.engine_budget = result.engine_budget.effective;

    bool used_candidate_source = false;
    if (sources_.engine_candidates) {
      auto bundle = sources_.engine_candidates(request, engine_plan);
      if (bundle.has_value()) {
        used_candidate_source = true;
        auto engine_item = std::move(bundle->engine_evidence);
        if (!engine_item.payload.empty()) {
          engine_item.kind = EvidenceKind::engine;
          engine_item.id = std::string(stable_id(EvidenceKind::engine));
          engine_item.confidence = std::clamp(engine_item.confidence, 0.0, 1.0);
          result.items.push_back(std::move(engine_item));
        }

        if (requested(plan, EvidenceKind::candidate_moves) &&
            !result.candidate_moves.has_value()) {
          append_candidates(result, bundle->candidate_snapshot, request);
        }
      }
    }
    if (!used_candidate_source) {
      append_source(result.items, sources_.engine, EvidenceKind::engine, request,
                    engine_plan);
    }
  }

  return result;
}

}  // namespace kchess::ai
