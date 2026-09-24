#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach LLM provider configuration
// -----------------------------------------------------------------------------

// Wire format of a provider. Several vendors share the OpenAI chat-completions
// shape, so the provider id and its wire format are configured separately.
enum class ProviderApi {
  gemini_interactions,
  anthropic_messages,
  openai_chat_completions,
};

enum class ProviderRequestPriority {
  automatic,
  repair,
  manual,
};

// The provider id selected in config/coach_provider.json names every local
// secret file: secrets/<id>_api_key.txt holds the key and
// secrets/<id>_usage.json the local quota state.
struct ProviderConfig {
  std::string id{"gemini"};
  ProviderApi api{ProviderApi::gemini_interactions};
  std::string endpoint;
  std::string api_key;
  std::string model;
  // Optional vendor effort/reasoning hint; empty keeps the vendor default.
  std::string effort;
  // openai_chat_completions only: name of the output-token limit field.
  std::string max_tokens_field{"max_tokens"};
  // openai_chat_completions only: "enabled"/"disabled" sent as
  // {"thinking": {"type": ...}}; empty leaves the vendor default.
  std::string thinking;
  // anthropic_messages only: let the API retry a refusal on its default model.
  bool refusal_fallback{false};
  int timeout_ms{45'000};
  int max_output_tokens{700};
  std::size_t rpm_soft_limit{10};
  std::size_t rpm_hard_limit{14};
  std::size_t tpm_soft_limit{200'000};
  std::size_t tpm_hard_limit{240'000};
  std::size_t rpd_soft_limit{450};
  std::size_t rpd_hard_limit{495};
  int backoff_base_seconds{2};
  int backoff_max_seconds{60};
  bool free_tier_only{true};
};

[[nodiscard]] std::optional<ProviderConfig> load_coach_provider_config(
    std::string* error_code = nullptr);

// -----------------------------------------------------------------------------
// Section: Local quota guard
// -----------------------------------------------------------------------------

[[nodiscard]] bool reserve_provider_request(
    const ProviderConfig& config,
    ProviderRequestPriority priority,
    std::size_t estimated_input_tokens,
    std::string* error_code = nullptr);
void record_provider_rate_limit(const ProviderConfig& config,
                                int retry_after_seconds = 0);
void record_provider_success(const ProviderConfig& config);

}  // namespace kchess::ai
