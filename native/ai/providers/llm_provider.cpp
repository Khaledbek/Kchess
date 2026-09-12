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
    const std::vector<EvidenceItem>& evidence) {
  LLMProviderRequest provider_request;
  provider_request.locale = context.locale;
  provider_request.intent = plan.intent;
  provider_request.mode = request.mode;
  provider_request.depth = plan.response_depth;
  provider_request.user_text = context.user_text;
  provider_request.position_fen = context.position_fen;
  provider_request.player_color = request.player_color;
  provider_request.hint_move_uci = request.hint_move_uci;
  provider_request.session_summary = context.session_summary;
  provider_request.pgn_excerpt = context.pgn_excerpt;
  provider_request.evidence = optimize_provider_evidence(
      evidence, plan.response_depth, context.estimated_tokens);
  provider_request.input_token_budget = context.input_token_budget;
  return provider_request;
}

}  // namespace kchess::ai
