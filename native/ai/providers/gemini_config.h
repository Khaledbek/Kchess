#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Local Gemini configuration
// -----------------------------------------------------------------------------

struct GeminiConfig {
  std::string api_key;
  std::string model{"gemini-3.5-flash-lite"};
  int timeout_ms{45'000};
  int max_output_tokens{700};
  std::size_t max_requests_per_24h{100};
  int min_seconds_between_requests{0};
  bool free_tier_only{true};
};

[[nodiscard]] std::optional<GeminiConfig> load_gemini_config(
    std::string* error_code = nullptr);
[[nodiscard]] bool reserve_gemini_request(const GeminiConfig& config,
                                          std::string* error_code = nullptr);

}  // namespace kchess::ai
