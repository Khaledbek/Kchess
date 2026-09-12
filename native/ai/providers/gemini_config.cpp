#include "gemini_config.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;
using Clock = std::chrono::system_clock;

fs::path project_root() {
#ifdef KCHESS_PROJECT_ROOT
  return fs::path(KCHESS_PROJECT_ROOT);
#else
  return fs::current_path();
#endif
}

std::string trimmed(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::optional<std::string> read_text(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::vector<std::int64_t> read_usage(const fs::path& path) {
  const auto text = read_text(path);
  if (!text) return {};
  try {
    const auto value = json::parse(*text);
    if (!value.contains("requests") || !value["requests"].is_array()) return {};
    return value["requests"].get<std::vector<std::int64_t>>();
  } catch (...) {
    return {};
  }
}

void write_usage(const fs::path& path, const std::vector<std::int64_t>& requests) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (output) output << json{{"requests", requests}}.dump();
}

std::mutex& usage_mutex() {
  static std::mutex mutex;
  return mutex;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Configuration loading
// -----------------------------------------------------------------------------

std::optional<GeminiConfig> load_gemini_config(std::string* error_code) {
  const auto root = project_root();
  const auto config_text = read_text(root / "config" / "coach_provider.json");
  if (!config_text) {
    if (error_code) *error_code = "gemini_config_missing";
    return std::nullopt;
  }

  GeminiConfig config;
  try {
    const auto value = json::parse(*config_text);
    if (value.value("provider", std::string{}) != "gemini") {
      if (error_code) *error_code = "gemini_provider_disabled";
      return std::nullopt;
    }
    config.model = value.value("model", config.model);
    config.timeout_ms = value.value("timeoutMs", config.timeout_ms);
    config.max_output_tokens = value.value("maxOutputTokens", config.max_output_tokens);
    config.max_requests_per_24h = value.value(
        "maxRequestsPer24Hours", config.max_requests_per_24h);
    config.min_seconds_between_requests = value.value(
        "minSecondsBetweenRequests", config.min_seconds_between_requests);
    config.free_tier_only = value.value("freeTierOnly", true);
    config.max_requests_per_24h = std::min<std::size_t>(100, std::max<std::size_t>(1, config.max_requests_per_24h));
    config.min_seconds_between_requests = std::max(0, config.min_seconds_between_requests);
    config.max_output_tokens = std::min(700, std::max(64, config.max_output_tokens));
    config.timeout_ms = std::min(60'000, std::max(5'000, config.timeout_ms));
  } catch (...) {
    if (error_code) *error_code = "gemini_config_invalid";
    return std::nullopt;
  }

  const auto key_text = read_text(root / "secrets" / "gemini_api_key.txt");
  config.api_key = key_text ? trimmed(*key_text) : std::string{};
  if (config.api_key.empty() || config.api_key == "PASTE_YOUR_GEMINI_AUTH_KEY_HERE") {
    if (error_code) *error_code = "gemini_api_key_missing";
    return std::nullopt;
  }
  if (!config.free_tier_only || config.model != "gemini-3.5-flash-lite") {
    if (error_code) *error_code = "gemini_free_tier_guard_config";
    return std::nullopt;
  }
  return config;
}

// -----------------------------------------------------------------------------
// Section: Conservative rolling quota guard
// -----------------------------------------------------------------------------

bool reserve_gemini_request(const GeminiConfig& config, std::string* error_code) {
  std::lock_guard lock(usage_mutex());
  const auto usage_path = project_root() / "secrets" / "gemini_usage.json";
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       Clock::now().time_since_epoch())
                       .count();
  const auto cutoff = now - 24 * 60 * 60;
  auto requests = read_usage(usage_path);
  requests.erase(std::remove_if(requests.begin(), requests.end(),
                                [cutoff](std::int64_t value) { return value < cutoff; }),
                 requests.end());

  if (requests.size() >= config.max_requests_per_24h) {
    write_usage(usage_path, requests);
    if (error_code) *error_code = "gemini_local_daily_guard";
    return false;
  }
  if (!requests.empty() &&
      now - requests.back() < config.min_seconds_between_requests) {
    if (error_code) *error_code = "gemini_local_rate_guard";
    return false;
  }

  requests.push_back(now);
  write_usage(usage_path, requests);
  return true;
}

}  // namespace kchess::ai
