#include "engine_budget.h"

#include <algorithm>

#include "experts/chess_expert_registry.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Existing engine grounding
// -----------------------------------------------------------------------------

bool has_existing_engine_grounding(const std::vector<EvidenceItem>& evidence) {
  return std::any_of(evidence.begin(), evidence.end(), [](const auto& item) {
    return item.kind == EvidenceKind::cache ||
           item.kind == EvidenceKind::existing_analysis ||
           item.kind == EvidenceKind::engine ||
           item.kind == EvidenceKind::candidate_moves;
  });
}


bool contains_need(const std::vector<EvidenceNeed>& needs, EvidenceNeed need) {
  return std::find(needs.begin(), needs.end(), need) != needs.end();
}

bool existing_analysis_satisfies_engine_plan(
    const QueryPlan& plan,
    const std::vector<EvidenceNeed>& satisfied_existing_needs) {
  bool requires_engine_fact = false;
  for (const auto need : plan.evidence_plan.needs) {
    if (!chess_expert_supports(EvidenceSource::engine, need)) continue;
    requires_engine_fact = true;
    if (!contains_need(satisfied_existing_needs, need)) return false;
  }
  return requires_engine_fact;
}

bool has_position_context(const CoachRequest& request) {
  return request.position_fen.has_value() || request.game_pgn.has_value();
}

EngineBudget refine_depth(const CoachRequest& request,
                          const QueryPlan& plan) {
  if (plan.engine_budget == EngineBudget::none) return EngineBudget::none;

  // EvidencePlan is the canonical deterministic depth request. Background turns
  // never escalate to a deep engine search merely because richer prose was
  // requested; foreground/critical requests may do so when deep evidence is required.
  if (plan.evidence_plan.priority != EvidencePlanPriority::background &&
      plan.evidence_plan.depth == EvidenceAnalysisDepth::deep) {
    return EngineBudget::deep;
  }

  const bool deep_review = request.mode == CoachMode::review ||
                           plan.intent == CoachIntent::game_review;
  const bool detailed_compare = request.mode == CoachMode::compare &&
                                plan.response_depth == ResponseDepth::detailed;
  return deep_review || detailed_compare ? EngineBudget::deep
                                         : EngineBudget::probe;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public budget policy
// -----------------------------------------------------------------------------

EngineBudgetDecision EngineBudgetSystem::decide(
    const CoachRequest& request,
    const QueryPlan& plan,
    const std::vector<EvidenceItem>& available,
    const std::vector<EvidenceNeed>& satisfied_existing_needs) const {
  EngineBudgetDecision decision;
  decision.requested = plan.engine_budget;

  if (!has_position_context(request) ||
      plan.engine_budget == EngineBudget::none) {
    return decision;
  }

  // A top-N hint/cache cannot prove the worst legal root move and cannot prove
  // the shortest forced loss among omitted legal moves. These modes therefore
  // require their dedicated complete-root scout even when ordinary engine or
  // candidate evidence for the same FEN already exists.
  if (plan.analysis_mode == PositionAnalysisMode::worst_move ||
      plan.analysis_mode == PositionAnalysisMode::fastest_loss) {
    decision.effective = refine_depth(request, plan);
    return decision;
  }

  const bool hard_native_requirement =
      plan.analysis_mode_source == AnalysisModeSource::native_state ||
      plan.analysis_mode_source == AnalysisModeSource::request_mode ||
      request.user_move_uci.has_value() ||
      request.verdict_challenge.has_value();

  // Freshness is a retrieval policy, never a replacement for hard native truth.
  // A structural move-evaluation request may still start the engine even when
  // cache-only reuse would otherwise be sufficient.
  if (!hard_native_requirement &&
      plan.evidence_plan.freshness == EvidenceFreshness::reuse_only) {
    if (existing_analysis_satisfies_engine_plan(
            plan, satisfied_existing_needs) ||
        (plan.evidence_plan.needs.empty() &&
         has_existing_engine_grounding(available))) {
      decision.satisfied_by_existing = true;
    }
    return decision;
  }

  // A plan that explicitly requires fresh information must not be satisfied
  // by stale/existing engine evidence alone. Hard native constraints already
  // flow through the same fresh path below.
  if (plan.evidence_plan.freshness == EvidenceFreshness::fresh_required) {
    decision.effective = refine_depth(request, plan);
    return decision;
  }

  if (existing_analysis_satisfies_engine_plan(
          plan, satisfied_existing_needs)) {
    decision.satisfied_by_existing = true;
    return decision;
  }

  // Backward-compatible fallback for legacy plans that do not expose any
  // planner-visible engine needs yet. Once a plan declares concrete needs,
  // only proven coverage above may suppress fresh engine work.
  if (plan.evidence_plan.needs.empty() &&
      has_existing_engine_grounding(available)) {
    decision.satisfied_by_existing = true;
    return decision;
  }

  decision.effective = refine_depth(request, plan);
  return decision;
}

}  // namespace kchess::ai
