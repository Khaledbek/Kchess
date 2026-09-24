#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../coach_types.h"
#include "../context_builder.h"
#include "../dto/coach_request.h"
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

struct LLMProviderRequest {
  std::string locale;
  CoachIntent intent{CoachIntent::unknown};
  CoachMode mode{CoachMode::answer};
  ResponseDepth depth{ResponseDepth::standard};
  std::string user_text;
  std::optional<std::string> position_fen;
  std::optional<std::string> player_color;
  std::optional<std::string> hint_move_uci;
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
  std::string response_schema_version{"coach_response.v4"};
  std::optional<StructuredCoachContent> repair_candidate;
  std::vector<std::string> validation_feedback;
  QueryFamily query_family{QueryFamily::unknown};
  bool needs_profile{false};
  ProfileQueryScope profile_scope;
};

enum class LLMProviderStatus {
  ok,
  unavailable,
  error,
};

struct LLMProviderResult {
  LLMProviderStatus status{LLMProviderStatus::unavailable};
  StructuredCoachContent content;
  std::string provider_id;
  std::string model_id;
  std::string error_code;

  [[nodiscard]] bool ok() const {
    return status == LLMProviderStatus::ok && !content.empty();
  }

  // Adapters report unparseable or truncated structured output as
  // "<provider>_response_invalid".
  [[nodiscard]] bool output_invalid() const {
    return error_code.ends_with("_response_invalid");
  }
};

// -----------------------------------------------------------------------------
// Section: Replaceable provider interface
// -----------------------------------------------------------------------------

class LLMProvider {
 public:
  virtual ~LLMProvider() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
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
