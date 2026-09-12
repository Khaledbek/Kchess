#include "engine_budget.h"

#include <algorithm>

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

bool has_position_context(const CoachRequest& request) {
  return request.position_fen.has_value() || request.game_pgn.has_value();
}

EngineBudget refine_depth(const CoachRequest& request,
                          const QueryPlan& plan) {
  if (plan.engine_budget == EngineBudget::none) return EngineBudget::none;

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
    const std::vector<EvidenceItem>& available) const {
  EngineBudgetDecision decision;
  decision.requested = plan.engine_budget;

  if (!has_position_context(request) ||
      plan.engine_budget == EngineBudget::none) {
    return decision;
  }

  if (has_existing_engine_grounding(available)) {
    decision.satisfied_by_existing = true;
    return decision;
  }

  decision.effective = refine_depth(request, plan);
  return decision;
}

}  // namespace kchess::ai
