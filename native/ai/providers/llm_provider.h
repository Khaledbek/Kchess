#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../coach_types.h"
#include "../context_builder.h"
#include "../dto/coach_request.h"
#include "../dto/chess_verdict.h"
#include "../dto/evidence.h"
#include "../dto/query_plan.h"
#include "../dto/structured_coach_response.h"
#include "../teaching/teaching_plan.h"

namespace kchess::ai {
struct ProviderInputOptimizationStats;
}

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider-neutral request/result contracts
// -----------------------------------------------------------------------------

inline constexpr std::string_view kCoachResponseSchemaVersion =
    "coach_response.v10";
inline constexpr std::string_view kCoachLLMContextSchemaVersion =
    "coach_llm_context.v6";


struct LLMInteractionContract {
  std::string request_kind{"question"};
  std::vector<std::string> requested_actions;
  std::string answer_intent{"answer"};
  std::optional<int> elo;
  std::optional<std::string> color;
  std::optional<std::string> game_mode;
  std::optional<std::string> time_control;
};

struct LLMProviderRequest {
  std::string locale;
  CoachIntent intent{CoachIntent::unknown};
  CoachMode mode{CoachMode::answer};
  ResponseDepth depth{ResponseDepth::standard};
  std::string user_text;
  LLMInteractionContract interaction;
  std::optional<ChessVerdictContract> chess_verdict;
  std::optional<ChessVerdictReview> verdict_review;
  std::optional<std::string> position_fen;
  std::optional<std::string> player_color;
  std::optional<std::string> hint_move_uci;
  std::optional<CoachMoveAttribution> move_attribution;
  std::optional<std::string> teaching_target;
  TeachingPlan teaching_plan;
  std::string session_summary;
  std::string pgn_excerpt;
  std::vector<EvidenceItem> evidence;
  std::size_t input_token_budget{0};
  bool automatic_turn{false};
  // Set by the native session after a completed legal move answered an open
  // question. Never infer this state from prior answer prose.
  bool has_verified_learner_feedback{false};
  std::string response_schema_version{std::string(kCoachResponseSchemaVersion)};
  std::string context_schema_version{std::string(kCoachLLMContextSchemaVersion)};
  std::optional<StructuredCoachContent> repair_candidate;
  std::vector<std::string> validation_feedback;
  QueryFamily query_family{QueryFamily::unknown};
  PositionAnalysisMode analysis_mode{PositionAnalysisMode::none};
  bool analysis_mode_explicit{false};
  // Planner-visible information requirements for this turn. This is context
  // for the coach LLM, not chess truth: only supplied evidence may support
  // concrete board claims.
  EvidencePlan evidence_plan;
  bool needs_profile{false};
  ProfileQueryScope profile_scope;
};

enum class LLMProviderStatus {
  ok,
  unavailable,
  error,
};

enum class LLMProviderFailureKind {
  none,
  unavailable,
  output_invalid,
  transport_error,
  quota,
  other,
};

struct LLMProviderResult {
  LLMProviderStatus status{LLMProviderStatus::unavailable};
  StructuredCoachContent content;
  std::string provider_id;
  std::string model_id;
  std::string error_code;
  LLMProviderFailureKind failure_kind{LLMProviderFailureKind::none};

  [[nodiscard]] bool ok() const {
    return status == LLMProviderStatus::ok && !content.empty();
  }
};

// -----------------------------------------------------------------------------
// Section: Replaceable provider interface
// -----------------------------------------------------------------------------

class LLMProvider {
 public:
  virtual ~LLMProvider() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  // Stable cache namespace for provider output. This must change whenever a
  // provider/model selection changes so validated prose from an older model is
  // never reused after a runtime configuration switch.
  [[nodiscard]] virtual std::string cache_identity() const {
    return std::string(id());
  }
  [[nodiscard]] virtual bool available() const noexcept = 0;
  [[nodiscard]] virtual LLMProviderResult complete(
      const LLMProviderRequest& request) const = 0;
};

[[nodiscard]] LLMProviderRequest make_llm_provider_request(
    const CoachRequest& request,
    const CoachContext& context,
    const QueryPlan& plan,
    const TeachingPlan& teaching_plan,
    const std::vector<EvidenceItem>& evidence,
    ProviderInputOptimizationStats* optimization_stats = nullptr);

}  // namespace kchess::ai
