#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "conversation/coach_session.h"
#include "evidence_retriever.h"
#include "optimization/validated_response_cache.h"
#include "dto/coach_request.h"
#include "dto/coach_response.h"
#include "providers/llm_provider.h"
#include "position/position_analysis_stage.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Read-only pipeline diagnostics
// -----------------------------------------------------------------------------

struct CoachPipelineTrace {
  bool automatic_turn{false};
  bool accepted{false};
  bool validation_passed{true};
  bool validation_repaired{false};
  std::uint64_t total_ms{0};
  std::uint64_t session_ms{0};
  std::uint64_t route_ms{0};
  std::uint64_t planning_ms{0};
  std::uint64_t context_ms{0};
  std::uint64_t retrieval_ms{0};
  std::uint64_t position_analysis_ms{0};
  bool position_cache_requested{false};
  bool position_cache_any_hit{false};
  bool position_cache_full_hit{false};
  bool response_cache_requested{false};
  bool response_cache_hit{false};
  std::uint64_t practicality_ms{0};
  std::uint64_t teaching_plan_ms{0};
  std::uint64_t provider_request_ms{0};
  std::uint64_t response_cache_key_ms{0};
  std::uint64_t provider_ms{0};
  std::uint64_t validation_ms{0};
  std::uint64_t repair_provider_ms{0};
  std::size_t retrieved_evidence_count{0};
  std::size_t final_evidence_count{0};
  std::size_t provider_evidence_count{0};
  std::size_t provider_evidence_input_count{0};
  std::size_t provider_exact_duplicates_dropped{0};
  std::size_t provider_compacted_items{0};
  std::size_t provider_compaction_savings_tokens{0};
  std::size_t provider_estimated_input_tokens{0};
  std::size_t provider_estimated_selected_tokens{0};
  std::size_t provider_evidence_budget_tokens{0};
  int provider_calls{0};
  std::string provider_error_code;
  std::vector<std::string> validation_issues;
  std::string safe_fallback_kind;
  std::string fallback_reason;
  std::string analysis_mode;
  bool analysis_mode_explicit{false};
  double evidence_plan_confidence{0.0};
  std::size_t evidence_plan_source_count{0};
  std::size_t evidence_plan_need_count{0};
  std::string evidence_plan_freshness;
  std::string evidence_plan_interaction;
  std::string evidence_plan_elo_target;
  std::size_t aggregated_evidence_count{0};
  std::size_t evidence_needs_satisfied{0};
  std::size_t evidence_needs_missing{0};
  std::size_t evidence_duplicates_removed{0};
  std::size_t evidence_conflicts_resolved{0};
  std::size_t native_candidate_count{0};
  std::size_t native_fact_count{0};
  std::string native_focus_kind;
  std::string routed_intent;
  std::string context_intent;
  bool explicit_current_intent{false};
  std::string teaching_objective;
  std::string teaching_delivery_mode;
  std::string teaching_skill_id;
  std::string learner_attempt_status;
  std::string learner_move_classification;
  std::string move_mover_color;
  std::string move_learner_color;
  std::string move_mover_role;
  std::string move_played_uci;
  bool move_verified_learner{false};
  std::string interaction_request_kind;
  std::string interaction_answer_intent;
  bool answer_llm_used{false};
  bool response_renderable{false};
  std::string native_answer_kind;
  bool verdict_authoritative{false};
  std::string verdict_position;
  std::string verdict_move;
  std::string verdict_evaluated_move;
  std::string verdict_basis;
  std::string verdict_review_outcome;
  std::string verdict_review_move;
  bool verdict_grounding_required{false};
  bool verdict_grounding_available{true};
  bool verdict_grounding_blocked{false};
  bool action_fulfillment_required{false};
  bool action_fulfillment_passed{true};
  std::vector<std::string> requested_actions;
  std::vector<std::string> transported_client_actions;
  std::vector<std::string> action_fulfillment_issues;
};

// -----------------------------------------------------------------------------
// Section: Coach pipeline entry point
// -----------------------------------------------------------------------------

class CoachOrchestrator {
 public:
  explicit CoachOrchestrator(
      EvidenceSources evidence_sources = {},
      std::shared_ptr<const LLMProvider> provider = {},
      std::function<void(const CoachLearningAttempt&)> record_learning = {},
      std::function<void(const CoachPipelineTrace&)> record_diagnostics = {});

  [[nodiscard]] CoachResponse handle(const CoachRequest& request) const;
  [[nodiscard]] bool has_pending_move_question(
      const std::optional<std::string>& session_id,
      const std::optional<std::string>& profile_id,
      const std::optional<std::string>& question_fen) const;
  void restore_session(const std::string& session_id,
                       const CoachSessionState& state) const;
  [[nodiscard]] std::optional<CoachSessionState> session_state(
      const std::optional<std::string>& session_id,
      const std::optional<std::string>& profile_id) const;
  [[nodiscard]] PositionAnalysisCacheStats position_cache_stats() const;
  [[nodiscard]] ValidatedResponseCacheStats response_cache_stats() const;

 private:
  EvidenceRetriever evidence_retriever_;
  mutable PositionAnalysisStage position_analysis_stage_;
  mutable ValidatedResponseCache response_cache_;
  std::shared_ptr<const LLMProvider> provider_;
  mutable CoachSessionMemory sessions_;
  std::function<void(const CoachLearningAttempt&)> record_learning_;
  std::function<void(const CoachPipelineTrace&)> record_diagnostics_;
};

}  // namespace kchess::ai
