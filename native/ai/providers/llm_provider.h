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
  std::string session_summary;
  std::string pgn_excerpt;
  std::vector<EvidenceItem> evidence;
  std::size_t input_token_budget{0};
  std::string response_schema_version{"coach_response.v2"};
  std::optional<StructuredCoachContent> repair_candidate;
  std::vector<std::string> validation_feedback;
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
    const std::vector<EvidenceItem>& evidence);

}  // namespace kchess::ai
