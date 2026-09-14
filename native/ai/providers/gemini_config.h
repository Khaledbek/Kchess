#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace kchess::ai {

enum class GeminiRequestPriority {
  automatic,
  repair,
  manual,
};

struct GeminiConfig {
  std::string api_key;
  std::string model{"gemini-3.5-flash-lite"};
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

[[nodiscard]] std::optional<GeminiConfig> load_gemini_config(
    std::string* error_code = nullptr);
[[nodiscard]] bool reserve_gemini_request(
    const GeminiConfig& config,
    GeminiRequestPriority priority,
    std::size_t estimated_input_tokens,
    std::string* error_code = nullptr);
void record_gemini_rate_limit(const GeminiConfig& config,
                              int retry_after_seconds = 0);
void record_gemini_success();

}  // namespace kchess::ai
