#include "llm_provider.h"

#include "../optimization/provider_input_optimizer.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider request assembly
// -----------------------------------------------------------------------------

LLMProviderRequest make_llm_provider_request(
    const CoachRequest& request,
    const CoachContext& context,
    const QueryPlan& plan,
    const TeachingPlan& teaching_plan,
    const std::vector<EvidenceItem>& evidence,
    ProviderInputOptimizationStats* optimization_stats) {
  LLMProviderRequest provider_request;
  provider_request.locale = context.locale;
  provider_request.intent = plan.intent;
  provider_request.query_family = plan.query_family;
  provider_request.analysis_mode = plan.analysis_mode;
  provider_request.analysis_mode_explicit = plan.analysis_mode_explicit;
  provider_request.evidence_plan = plan.evidence_plan;
  provider_request.needs_profile = plan.needs_profile;
  if (plan.needs_profile) provider_request.profile_scope = plan.profile_scope;
  provider_request.mode = request.mode;
  provider_request.depth = plan.response_depth;
  provider_request.user_text = context.user_text;
  provider_request.position_fen = context.position_fen;
  if (plan.needs_position) {
    provider_request.player_color = request.player_color;
    provider_request.hint_move_uci = request.hint_move_uci;
  }
  provider_request.teaching_plan = teaching_plan;
  if (!teaching_plan.target.empty()) {
    provider_request.teaching_target = teaching_plan.target;
  } else {
    provider_request.teaching_target = request.teaching_target;
  }
  provider_request.session_summary = context.session_summary;
  provider_request.pgn_excerpt = context.pgn_excerpt;
  provider_request.evidence = optimize_provider_evidence(
      evidence, plan.response_depth, request.mode, context.estimated_tokens, &plan,
      &teaching_plan, optimization_stats);
  provider_request.input_token_budget = context.input_token_budget;
  provider_request.automatic_turn = request.automatic_turn;
  provider_request.move_attribution = request.move_attribution;
  return provider_request;
}

}  // namespace kchess::ai
