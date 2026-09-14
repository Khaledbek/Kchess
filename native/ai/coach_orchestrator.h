#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "conversation/coach_session.h"
#include "evidence_retriever.h"
#include "models/small_models.h"
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
  std::size_t provider_estimated_input_tokens{0};
  std::size_t provider_estimated_selected_tokens{0};
  std::size_t provider_evidence_budget_tokens{0};
  int provider_calls{0};
  std::string provider_error_code;
  std::vector<std::string> validation_issues;
  std::string safe_fallback_kind;
  std::string teaching_objective;
  std::string teaching_delivery_mode;
  std::string teaching_skill_id;
  std::string learner_attempt_status;
  std::string learner_move_classification;
};

// -----------------------------------------------------------------------------
// Section: Coach pipeline entry point
// -----------------------------------------------------------------------------

class CoachOrchestrator {
 public:
  explicit CoachOrchestrator(
      EvidenceSources evidence_sources = {},
      std::shared_ptr<const LLMProvider> provider = {},
      SmallModelSuite small_models = {},
      std::function<void(const CoachLearningAttempt&)> record_learning = {},
      std::function<void(const CoachPipelineTrace&)> record_diagnostics = {});

  [[nodiscard]] CoachResponse handle(const CoachRequest& request) const;
  [[nodiscard]] bool has_pending_move_question(
      const std::optional<std::string>& session_id,
      const std::optional<std::string>& profile_id,
      const std::optional<std::string>& question_fen) const;
  [[nodiscard]] PositionAnalysisCacheStats position_cache_stats() const;
  [[nodiscard]] ValidatedResponseCacheStats response_cache_stats() const;

 private:
  EvidenceRetriever evidence_retriever_;
  mutable PositionAnalysisStage position_analysis_stage_;
  mutable ValidatedResponseCache response_cache_;
  std::shared_ptr<const LLMProvider> provider_;
  SmallModelSuite small_models_;
  mutable CoachSessionMemory sessions_;
  std::function<void(const CoachLearningAttempt&)> record_learning_;
  std::function<void(const CoachPipelineTrace&)> record_diagnostics_;
};

}  // namespace kchess::ai
